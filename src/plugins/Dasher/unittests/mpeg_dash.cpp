#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_media/common/attributes.hpp"
#include "lib_media/common/metadata_file.hpp"
#include "lib_utils/tools.hpp" // safe_cast
#include "plugins/Dasher/mpeg_dash.hpp" // DasherConfig

using namespace Tests;
using namespace Modules;
using namespace std;

namespace {

std::shared_ptr<DataBase> createPacket(SpanC raw) {
	auto pkt = make_shared<DataRaw>(raw.len);
	memcpy(pkt->buffer->data().ptr, raw.ptr, raw.len);
	return pkt;
}

std::shared_ptr<DataBase> getTestSegment() {
	static const uint8_t markerData[] = { 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };

	auto r = createPacket(markerData);

	auto meta = make_shared<MetadataFile>(VIDEO_PKT);
	meta->durationIn180k = IClock::Rate * 3;
	r->setMetadata(meta);

	return r;
}

struct MyOutput : ModuleS {
	void processOne(Data) override {
		++frameCount;
	}
	int frameCount = 0;
};

}

unittest("dasher: simple live") {

	DasherConfig cfg {};
	cfg.segDurationInMs = 3000;
	cfg.live = true;
	auto dasher = loadModule("MPEG_DASH", &NullHost, &cfg);

	auto recSeg = createModule<MyOutput>();
	ConnectOutputToInput(dasher->getOutput(0), recSeg->getInput(0));

	auto recMpd = createModule<MyOutput>();
	ConnectOutputToInput(dasher->getOutput(1), recMpd->getInput(0));

	dasher->getInput(0)->connect();

	for(int i=0; i < 50; ++i) {
		auto s = getTestSegment();
		dasher->getInput(0)->push(s);
		dasher->process();
	}

	// We're live: the manifest must have been received before flushing
	ASSERT_EQUALS(50, recMpd->frameCount);

	// All the segments must have been received
	ASSERT_EQUALS(50, recSeg->frameCount);

	dasher->flush();

	// FIXME: the last segment gets uploaded twice
	ASSERT_EQUALS(50 + 1, recSeg->frameCount);
}

unittest("dasher: two live streams") {

	DasherConfig cfg {};
	cfg.segDurationInMs = 2000;
	cfg.live = true;
	auto dasher = loadModule("MPEG_DASH", &NullHost, &cfg);

	auto recSeg = createModule<MyOutput>();
	ConnectOutputToInput(dasher->getOutput(0), recSeg->getInput(0));

	auto recMpd = createModule<MyOutput>();
	ConnectOutputToInput(dasher->getOutput(1), recMpd->getInput(0));

	dasher->getInput(0)->connect();
	dasher->getInput(1)->connect();

	for(int i=0; i < 50; ++i) {
		auto s = getTestSegment();
		dasher->getInput(i%2)->push(s);
		dasher->process();
	}

	// We're live: the manifest must have been received before flushing
	ASSERT_EQUALS(25, recMpd->frameCount);

	// All the segments must have been received
	ASSERT_EQUALS(50, recSeg->frameCount);
}

unittest("dasher: timeshift buffer") {

	struct Server : ModuleS {
		void processOne(Data data) override {
			auto meta = safe_cast<const MetadataFile>(data->getMetadata().get());
			if(meta->filesize == INT64_MAX) // is it a "DELETE" ?
				deleted ++;
			else
				added ++;
		}
		int added = 0;
		int deleted = 0;
	};

	DasherConfig cfg {};
	cfg.segDurationInMs = 3000;
	cfg.timeShiftBufferDepthInMs = 9000;
	cfg.live = true;
	auto dasher = loadModule("MPEG_DASH", &NullHost, &cfg);

	auto server = createModule<Server>();
	ConnectOutputToInput(dasher->getOutput(0), server->getInput(0));

	dasher->getInput(0)->connect();

	for(int i=0; i < 40; ++i) {
		auto s = getTestSegment();
		dasher->getInput(0)->push(s);
		dasher->process();
	}
	ASSERT_EQUALS(40, server->added);
	ASSERT_EQUALS(36, server->deleted);
}

