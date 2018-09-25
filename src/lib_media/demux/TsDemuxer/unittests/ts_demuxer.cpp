#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_modules/utils/loader.hpp"
#include "../ts_demuxer.hpp"
#include "string.h" // memcpy

using namespace Tests;
using namespace Modules;

template<size_t N>
std::shared_ptr<DataBase> createPacket(uint8_t const (&bytes)[N]) {
	auto pkt = make_shared<DataRaw>(N);
	memcpy(pkt->data().ptr, bytes, N);
	return pkt;
}

namespace {
struct BitWriter {
	Span dst;

	void u(int n, int val) {
		for(int i=0; i < n; ++i) {
			int bit = (val >> (n-1-i)) & 1;
			putBit(bit);
		}
	}

	void putBit(int bit) {
		auto bitIndex = m_pos%8;
		auto byteIndex = m_pos/8;
		auto mask = (1 << (7-bitIndex));
		if(bit)
			dst.ptr[byteIndex] |= mask;
		else
			dst.ptr[byteIndex] &= ~mask;
		m_pos++;
	}

	int m_pos = 0;
};

std::shared_ptr<DataBase> getTestTs() {
	uint8_t tsPackets[2 * 188] {};
	BitWriter w { {tsPackets, sizeof tsPackets} };

	{
		w.u(8, 0x47); // sync byte
		w.u(1, 0); // TEI
		w.u(1, 1); // PUSI
		w.u(1, 0); // priority
		w.u(13, 120); // PID
		w.u(2, 0); // scrambling control
		w.u(2, 0b11); // adaptation field control
		w.u(4, 0); // continuity counter

		// adaptation field
		w.u(8, 3); // adaptation field length
		for(int i=0; i < 3; ++i)
			w.u(8, 0x99); // adaptation field raw byte

		// payload
		while(w.m_pos/8 < 188)
			w.u(8, 0x77);
	}

	{
		w.u(8, 0x47); // sync byte
		w.u(1, 0); // TEI
		w.u(1, 1); // PUSI
		w.u(1, 0); // priority
		w.u(13, 120); // PID
		w.u(2, 0); // scrambling control
		w.u(2, 0b01); // adaptation field control
		w.u(4, 0); // continuity counter
		for(int i=0; i < 184; ++i)
			w.u(8, 0x88);
	}

	return createPacket(tsPackets);
}

struct FrameCounter : ModuleS {
	FrameCounter() {
		addInput(this);
	}
	void process(Data data) override {
		++frameCount;
		totalLength += (int)data->data().len;
	}
	int frameCount = 0;
	int totalLength = 0;
};

}

unittest("TsDemuxer: simple") {
	TsDemuxerConfig cfg;
	cfg.pids[0].pid = 120;
	cfg.pids[0].type = 1;

	auto demux = loadModule("TsDemuxer", &NullHost, &cfg);
	auto rec = create<FrameCounter>();
	ConnectOutputToInput(demux->getOutput(0), rec->getInput(0));

	auto frame = getTestTs();
	demux->getInput(0)->push(frame);
	demux->process();
	demux->flush();

	ASSERT_EQUALS(2, rec->frameCount);
	ASSERT_EQUALS(184 - 4 + 184, rec->totalLength);
}

unittest("TsDemuxer: keep only one PID") {
	TsDemuxerConfig cfg;
	cfg.pids[0].pid = 130;
	cfg.pids[0].type = 1;
	cfg.pids[1].pid = 120;
	cfg.pids[1].type = 1;

	auto demux = loadModule("TsDemuxer", &NullHost, &cfg);
	auto rec130 = create<FrameCounter>();
	auto rec120 = create<FrameCounter>();
	ConnectOutputToInput(demux->getOutput(0), rec130->getInput(0));
	ConnectOutputToInput(demux->getOutput(1), rec120->getInput(0));

	auto frame = getTestTs();
	demux->getInput(0)->push(frame);
	demux->process();

	// don't flush

	ASSERT_EQUALS(0, rec130->frameCount);
	ASSERT_EQUALS(1, rec120->frameCount);
}

unittest("TsDemuxer: keep two PIDs") {

	uint8_t tsPackets[2 * 188] {};
	BitWriter w { {tsPackets, sizeof tsPackets} };

	{
		w.u(8, 0x47); // sync byte
		w.u(1, 0); // TEI
		w.u(1, 1); // PUSI
		w.u(1, 0); // priority
		w.u(13, 666); // PID
		w.u(2, 0); // scrambling control
		w.u(2, 0b01); // adaptation field control
		w.u(4, 0); // continuity counter

		// payload
		while(w.m_pos/8 < 188)
			w.u(8, 0x77);
	}

	{
		w.u(8, 0x47); // sync byte
		w.u(1, 0); // TEI
		w.u(1, 1); // PUSI
		w.u(1, 0); // priority
		w.u(13, 777); // PID
		w.u(2, 0); // scrambling control
		w.u(2, 0b01); // adaptation field control
		w.u(4, 0); // continuity counter

		// payload
		while(w.m_pos/8 < 188)
			w.u(8, 0x77);
	}

	TsDemuxerConfig cfg;
	cfg.pids[0].pid = 666;
	cfg.pids[0].type = 1;
	cfg.pids[1].pid = 777;
	cfg.pids[1].type = 1;

	auto demux = loadModule("TsDemuxer", &NullHost, &cfg);
	auto pid0 = create<FrameCounter>();
	auto pid1 = create<FrameCounter>();
	ConnectOutputToInput(demux->getOutput(0), pid0->getInput(0));
	ConnectOutputToInput(demux->getOutput(1), pid1->getInput(0));

	demux->getInput(0)->push(createPacket(tsPackets));
	demux->process();

	demux->flush();

	ASSERT_EQUALS(1, pid0->frameCount);
	ASSERT_EQUALS(1, pid1->frameCount);
}

