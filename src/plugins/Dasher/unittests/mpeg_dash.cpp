#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_media/common/attributes.hpp"
#include "lib_media/common/metadata_file.hpp"
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
	r->setMetadata(meta);

	return r;
}
}

unittest("dasher: simple") {

	struct MyOutput : ModuleS {
		void processOne(Data) override {
			++frameCount;
		}
		int frameCount = 0;
	};

	DasherConfig cfg {};
	cfg.segDurationInMs = 3000;
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

	dasher->flush();

	ASSERT_EQUALS(51, recSeg->frameCount);
	ASSERT_EQUALS(1, recMpd->frameCount);
}


