// MPEG-TS push demuxer.
//
// The design goal here is to discard the biggest amount of input
// data (which is often erroneous) while still getting the job done.
//
#include "ts_demuxer.hpp"
#include "lib_modules/utils/factory.hpp" // registerModule
#include "lib_modules/utils/helper.hpp"
#include "lib_media/common/metadata.hpp"
#include "lib_utils/log_sink.hpp" // Error
#include "lib_utils/format.hpp"
#include <vector>
#include <string.h> // memcpy
using namespace std;

using namespace Modules;

namespace {

auto const TS_PACKET_LEN = 188;
auto const PID_PAT = 0;

auto const TABLE_ID_PAT = 0;
auto const TABLE_ID_PMT = 2;

struct BitReader {
	SpanC src;

	int u(int n) {
		const int firstByte = m_pos/8;
		const int lastByte = (m_pos+n-1)/8;
		m_pos += n;

		uint64_t acc = 0;

		for(int k = firstByte; k <= lastByte; ++k) {
			acc <<= 8;
			acc |= src[k];
		}

		auto mask = ((1u << n)-1);
		auto shift = m_pos % 8 ? 8 - m_pos % 8 : 0;
		return (acc >> shift) & mask;
	}

	int byteOffset() const {
		assert(m_pos%8 == 0);
		return m_pos/8;
	}

	int remaining() const {
		return (int)src.len - m_pos/8;
	}

	SpanC payload() const {
		assert(m_pos%8 == 0);
		auto r = src;
		r += m_pos/8;
		return r;
	}

	bool empty() const {
		return size_t(m_pos/8) >= src.len;
	}

	int m_pos = 0;
};

struct Stream {
	Stream(int pid_) : pid(pid_) {}
	virtual ~Stream() = default;

	// send data for processing
	virtual void push(SpanC data) = 0;

	// tell the stream when the payload unit is finished (e.g PUSI=1 or EOS)
	virtual void flush() = 0;

	int pid = TsDemuxerConfig::ANY;
};

struct PsiStream : Stream {

		struct EsInfo {
			int pid, streamType;
		};

		struct Listener {
			virtual void onPat(span<int> pmtPids) = 0;
			virtual void onPmt(span<EsInfo> esInfo) = 0;
		};

		PsiStream(int pid_, Listener* listener_) : Stream(pid_), listener(listener_) {
		}

		void push(SpanC data) override {
			auto const PSI_HEADER_SIZE = 8;
			BitReader r = {data};
			if(r.remaining() < PSI_HEADER_SIZE)
				return; // truncated PSI header

			auto const table_id = r.u(8);
			/*auto const section_syntax_indicator =*/ r.u(1);
			/*auto const private_bit =*/ r.u(1);
			/*auto const reserved1 =*/ r.u(2);
			auto const section_length = r.u(12);

			auto sectionStart = r.byteOffset();

			/*auto const table_id_extension =*/ r.u(16);
			/*auto const reserved2 =*/ r.u(2);
			/*auto const version_number =*/ r.u(5);
			/*auto const current_next_indicator =*/ r.u(1);
			/*auto const section_number =*/ r.u(8);
			/*auto const last_section_number =*/ r.u(8);

			assert(PSI_HEADER_SIZE == r.byteOffset());

			switch(table_id) {
			case TABLE_ID_PAT: {
				// actual PAT data
				/*auto const program_number =*/ r.u(16);
				/*auto const reserved3 =*/ r.u(3);
				auto const program_map_pid = r.u(13);

				int pids[] = { program_map_pid };
				listener->onPat(pids);
				break;
			}
			case TABLE_ID_PMT: {
				/*auto const reserved3 =*/ r.u(3);
				/*auto const pcr_pid =*/ r.u(13);
				/*auto const reserved4 =*/ r.u(4);
				/*auto const program_info_length =*/ r.u(12);

				vector<EsInfo> info;

				while(r.byteOffset() < sectionStart + section_length) {
					// Elementary stream info
					auto const stream_type = r.u(8);
					/*auto const reserved5 =*/ r.u(3);
					auto const pid = r.u(13);
					/*auto const reserved6 =*/ r.u(4);
					auto const es_info_length = r.u(12);

					// skip es_info
					for(int i=0; i < es_info_length; ++i)
						r.u(8);

					info.push_back({ pid, stream_type });
				}

				listener->onPmt({info.data(), info.size()});
				break;
			}
			break;
			default:
				break;
			}
		}

		void flush() override {
		}

	private:
		Listener* const listener;
};

struct PesStream : Stream {
		PesStream(int pid_, int type_, OutputDefault* output_) : Stream(pid_), type(type_), m_output(output_) {
		}

		void push(SpanC data) override {
			for(auto b : data)
				pesBuffer.push_back(b);
		}

		void flush() override {
			if(pesBuffer.empty())
				return;

			auto buf = m_output->getBuffer(pesBuffer.size());
			memcpy(buf->data().ptr, pesBuffer.data(),pesBuffer.size());
			m_output->emit(buf);

			pesBuffer.clear();
		}

		bool setType(int streamType) {
			switch(streamType) {
			case 0x02: {
				auto meta = make_shared<MetadataPkt>(VIDEO_PKT);
				meta->codec = "m2v";
				m_output->setMetadata(meta);
				break;
			}
			case 0x04: {
				auto meta = make_shared<MetadataPkt>(AUDIO_PKT);
				meta->codec = "m2a";
				m_output->setMetadata(meta);
				break;
			}
			case 0x1b: {
				auto meta = make_shared<MetadataPkt>(VIDEO_PKT);
				meta->codec = "h264";
				m_output->setMetadata(meta);
				break;
			}
			case 0x24: {
				auto meta = make_shared<MetadataPkt>(VIDEO_PKT);
				meta->codec = "hevc";
				m_output->setMetadata(meta);
				break;
			}
			default:
				return false; // unknown stream type
			}
			return true;
		}

		int type;
	private:
		OutputDefault* m_output = nullptr;
		vector<uint8_t> pesBuffer;
};

struct TsDemuxer : ModuleS, PsiStream::Listener {
		TsDemuxer(IModuleHost* host, TsDemuxerConfig const& config)
			: m_host(host) {

			addInput(this);

			m_streams.push_back(make_unique<PsiStream>(PID_PAT, this));

			for(auto& pid : config.pids)
				if(pid.type != TsDemuxerConfig::NONE)
					m_streams.push_back(make_unique<PesStream>(pid.pid, pid.type, addOutput<OutputDefault>()));
		}

		void process(Data data) override {
			auto buf = data->data();

			while(buf.len > 0) {
				if(buf.len < TS_PACKET_LEN) {
					m_host->log(Error, "Truncated TS packet");
					return;
				}

				processTsPacket({buf.ptr, TS_PACKET_LEN});
				buf += TS_PACKET_LEN;
			}
		}

		void flush() {
			for(auto& s : m_streams)
				s->flush();
		}

		void processTsPacket(SpanC pkt) {
			BitReader r = {pkt};
			const int syncByte = r.u(8);
			const int transportErrorIndicator = r.u(1);
			const int payloadUnitStartIndicator = r.u(1);
			/*const int priority =*/ r.u(1);
			const int packetId = r.u(13);
			const int scrambling = r.u(2);
			const int adaptationFieldControl = r.u(2);
			/*const int continuityCounter =*/ r.u(4);

			if(syncByte != 0x47) {
				m_host->log(Error, "TS sync byte not found");
				return;
			}

			// skip adaptation field if any
			if(adaptationFieldControl & 0x2) {
				auto length = r.u(8);
				if(length > r.remaining()) {
					m_host->log(Error, "Invalid adaptation_field length in TS header");
					return;
				}
				for(int i=0; i < length; ++i)
					r.u(8);
			}

			auto stream = findStreamForPid(packetId);
			if(!stream)
				return; // we're not interested in this PID

			if(transportErrorIndicator) {
				m_host->log(Error, "Discarding TS packet with TEI=1");
				return;
			}

			if(scrambling) {
				m_host->log(Error, "Discarding scrambled TS packet");
				return;
			}

			if(payloadUnitStartIndicator)
				stream->flush();

			if(adaptationFieldControl & 0x1) {
				if(payloadUnitStartIndicator) {
					int pointerField = r.u(8);
					if(pointerField > r.remaining()) {
						m_host->log(Error, "Invalid pointer_field before TS payload");
						return;
					}
					for(int i=0; i < pointerField; ++i)
						r.u(8);
				}

				stream->push(r.payload());
			}
		}

		// PsiStream::Listener implementation
		void onPat(span<int> pmtPids) override {
			m_host->log(Debug, format("Found PAT (%s programs)", pmtPids.len).c_str());
			for(auto pid : pmtPids)
				m_streams.push_back(make_unique<PsiStream>(pid, this));
		}

		void onPmt(span<PsiStream::EsInfo> esInfo) override {
			m_host->log(Debug, format("Found PMT (%s streams)", esInfo.len).c_str());
			for(auto es : esInfo) {
				if(auto stream = findMatchingStream(es)) {
					stream->pid = es.pid;
					if(!stream->setType(es.streamType))
						m_host->log(Warning, format("Unknown stream type for PID=%s: %s", es.pid, es.streamType).c_str());
				}
			}
		}

		PesStream* findMatchingStream(PsiStream::EsInfo es) {
			for(auto& s : m_streams) {
				if(auto stream = dynamic_cast<PesStream*>(s.get()))
					if(matches(stream, es))
						return stream;
			}

			return nullptr;
		}

		static bool matches(PesStream* stream, PsiStream::EsInfo es) {
			if(stream->pid == TsDemuxerConfig::ANY) {
				switch(stream->type) {
				case TsDemuxerConfig::AUDIO:
					return es.streamType == 0x4;
				case TsDemuxerConfig::VIDEO:
					return es.streamType == 0x2 || es.streamType == 0x1b || es.streamType == 0x24;
				default:
					assert(0);
					return false;
				}
			} else {
				return stream->pid == es.pid;
			}
		}

	private:
		Stream* findStreamForPid(int packetId) {
			for(auto& s : m_streams) {
				if(s->pid == packetId)
					return s.get();
			}
			return nullptr;
		}

		IModuleHost* const m_host;
		vector<unique_ptr<Stream>> m_streams;
};

Modules::IModule* createObject(IModuleHost* host, va_list va) {
	auto config = va_arg(va, TsDemuxerConfig*);
	enforce(host, "TsDemuxer: host can't be NULL");
	enforce(config, "TsDemuxer: config can't be NULL");
	return Modules::create<TsDemuxer>(host, *config).release();
}
}

auto const registered = Factory::registerModule("TsDemuxer", &createObject);

