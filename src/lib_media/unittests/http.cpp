#include "tests/tests.hpp"
#include "lib_media/out/http.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_modules/modules.hpp"
#include <string.h> // memcpy

using namespace Modules;

namespace {

std::shared_ptr<DataBase> createPacket(span<const char> contents) {
	auto r = make_shared<DataRaw>(contents.len);
	memcpy(r->data().ptr, contents.ptr, contents.len);
	return r;
}

}

secondclasstest("HTTP post") {
	HttpOutputConfig cfg {};
	cfg.url = "http://example.com";
	auto mod = loadModule("HTTP", &NullHost, &cfg);
	mod->getInput(0)->push(createPacket("Hello World"));
	mod->getInput(0)->push(createPacket("Goodbye World"));
	mod->process();
	mod->flush();
}

