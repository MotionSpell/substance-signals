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
	uint8_t data[] = {
		// 'ftyp' box
		0x00, 0x00, 0x00, 0x0A,
		'f', 't', 'y', 'p',
		0x77, 0x77,

		// 'evil' box
		0x00, 0x00, 0x00, 0x0C,
		'e', 'v', 'i', 'l',
		0x66, 0x66, 0x66, 0x66,

		// 'free' box
		0x00, 0x00, 0x00, 0x0A,
		'f', 'r', 'e', 'e',
		0x55, 0x55,

		// 'moov' box
		0x00, 0x00, 0x00, 0x10,
		'm', 'o', 'o', 'v',
		0x11, 0x22, 0x22, 0x22,
		0x22, 0x22, 0x22, 0x33,

		// actual data
		0xAA, 0xAA, 0xAA, 0xAA, 0xFF,
	};
	mod->process(createPacket(data));
	mod->process(createPacket(data));
	mod->process(createPacket(data));
	//mod->flush();
}

