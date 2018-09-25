#include "ts_demuxer.hpp"
#include "lib_modules/utils/factory.hpp" // registerModule
#include "lib_modules/utils/helper.hpp"
#include "lib_utils/log_sink.hpp" // Error
#include <vector>
#include <string.h> // memcpy
using namespace std;

using namespace Modules;

namespace {

auto const TS_PACKET_LEN = 188;

struct BitReader {
	SpanC src;

	int u(int n) {
		int r = 0;
		for(int i=0; i < n; ++i) {
			r <<= 1;
			r |= bit();
		}
		return r;
	}

	bool empty() const {
		return size_t(m_pos/8) >= src.len;
	}

	int bit() {
		auto bitIndex = m_pos%8;
		auto byteIndex = m_pos/8;
		int bit = (src.ptr[byteIndex] >> (7-bitIndex)) & 1;
		m_pos++;
		return bit;
	}

	int m_pos = 0;
};

struct Stream {
	const int pid;
	OutputDefault* m_output = nullptr;
	vector<uint8_t> pesBuffer {};

	void flush() {
		if(pesBuffer.empty())
			return;

		auto buf = m_output->getBuffer(pesBuffer.size());
		memcpy(buf->data().ptr, pesBuffer.data(),pesBuffer.size());
		m_output->emit(buf);

		pesBuffer.clear();
	}
};

struct TsDemuxer : ModuleS {
	TsDemuxer(IModuleHost* host, TsDemuxerConfig const& config)
		: m_host(host) {

		addInput(this);

		for(auto& pid : config.pids) {
			m_streams.push_back( { pid.pid, addOutput<OutputDefault>() } );
		}
	}

	void process(Data data) override {

		assert(data);
		auto buf = data->data();

		while(buf.len > 0) {
			if(buf.len < TS_PACKET_LEN) {
				m_host->log(Error, "Truncated TS packet");
				return;
			}

			processTsPacket({buf.ptr, TS_PACKET_LEN});

			buf.ptr += TS_PACKET_LEN;
			buf.len -= TS_PACKET_LEN;
		}
	}

	void flush() {
		for(auto& s : m_streams)
			s.flush();
	}

	void processTsPacket(SpanC pkt) {
		BitReader r = {pkt};
		const int syncByte = r.u(8);
		const int transportErrorIndicator = r.u(1);
		const int payloadUnitStartIndicator = r.u(1);
		const int priority = r.u(1);
		const int packetId = r.u(13);
		const int scrambling = r.u(2);
		const int adaptationFieldControl = r.u(2);
		const int continuityCounter = r.u(4);

		(void)transportErrorIndicator;
		(void)payloadUnitStartIndicator;
		(void)priority;
		(void)scrambling;
		(void)continuityCounter;

		if(syncByte != 0x47) {
			m_host->log(Error, "TS sync byte not found");
			return;
		}

		// skip adaptation field if any
		if(adaptationFieldControl & 0x2) {
			auto length = r.u(8);
			for(int i=0; i < length; ++i)
				r.u(8);
		}

		auto stream = findStreamForPid(packetId);
		if(!stream)
			return; // we're not interested in this PID

		if(payloadUnitStartIndicator)
			stream->flush();

		if(adaptationFieldControl & 0x1) {
			while(!r.empty())
				stream->pesBuffer.push_back(r.u(8));
		}
	}

	Stream* findStreamForPid(int packetId) {
		for(auto& s : m_streams) {
			if(s.pid == packetId)
				return &s;
		}
		return nullptr;
	}

	IModuleHost* const m_host;
	vector<Stream> m_streams;
};

Modules::IModule* createObject(IModuleHost* host, va_list va) {
	auto config = va_arg(va, TsDemuxerConfig*);
	enforce(host, "TsDemuxer: host can't be NULL");
	enforce(config, "TsDemuxer: config can't be NULL");
	return Modules::create<TsDemuxer>(host, *config).release();
}
}

auto const registered = Factory::registerModule("TsDemuxer", &createObject);

