#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_media/common/metadata.hpp"
#include "lib_media/transform/telx2ttml.hpp"
#include <string.h>

using namespace Tests;
using namespace Modules;

namespace {

template<size_t numBytes>
std::shared_ptr<DataBase> createPacket(uint8_t const (&bytes)[numBytes]) {
	auto pkt = make_shared<DataRaw>(numBytes);
	memcpy(pkt->data().ptr, bytes, numBytes);
	return pkt;
}

std::shared_ptr<DataBase> getTeletextTestFrame() {
	static const uint8_t teletext[] = {
		// garbage data
		0xde, 0xad, 0xbe, 0xef, 0x4a, 0xce, 0x00, 0x00,
	};

	auto r= createPacket(teletext);
	r->setMetadata(make_shared<MetadataPkt>(SUBTITLE_PKT));
	return r;
}
}
unittest("telx2ttml: simple") {
	TeletextToTtmlConfig cfg;
	auto reader = loadModule("TeletextToTTML", &NullHost, &cfg);
	reader->getInput(0)->push(getTeletextTestFrame());
	reader->process();
}

