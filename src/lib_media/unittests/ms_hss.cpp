#include "tests/tests.hpp"
#include "lib_media/common/metadata.hpp"
#include "lib_media/stream/ms_hss.hpp"
#include "lib_modules/modules.hpp"
#include <string.h> // memcpy

using namespace Modules;
using namespace Stream;

namespace {

std::shared_ptr<DataBase> createPacket(span<uint8_t> contents) {
	auto meta = make_shared<MetadataFile>( "filename", VIDEO_PKT, "mimetype", "codecName", 100, 0, 0, false, false);
	auto r = make_shared<DataRaw>(contents.len);
	r->setMetadata(meta);
	memcpy(r->data().ptr, contents.ptr, contents.len);
	return r;
}

}


secondclasstest("MS_HSS: simple") {
	auto mod = create<MS_HSS>(&NullHost, "http://127.0.0.1:9000");
	uint8_t data[256] = { 0x00, 0x10 };
	mod->process(createPacket(data));
	mod->process(createPacket(data));
	mod->process(createPacket(data));
	//mod->flush();
}

