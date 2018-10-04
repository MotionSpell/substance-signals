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

unittest("HTTP: should fail if the server doesn't exist") {
	HttpOutputConfig cfg {};
	cfg.flags.InitialEmptyPost = true;
	cfg.url = "http://unexisting_domain_name/nonexisting_page:1234";
	ASSERT_THROWN(loadModule("HTTP", &NullHost, &cfg));
}

secondclasstest("HTTP: empty post should succeed if the server exists") {
	HttpOutputConfig cfg {};
	cfg.flags.InitialEmptyPost = true;
	cfg.url = "http://127.0.0.1:9000";
	loadModule("HTTP", &NullHost, &cfg);
}

secondclasstest("HTTP: post data to real server") {
	HttpOutputConfig cfg {};
	cfg.flags.InitialEmptyPost = false;
	cfg.url = "http://127.0.0.1:9000";
	auto mod = loadModule("HTTP", &NullHost, &cfg);
	mod->getInput(0)->push(createPacket("Hello"));
	mod->getInput(0)->push(createPacket("Goodbye"));
	mod->process();
	mod->flush();
}

secondclasstest("HTTP: post data to real server, suffix but no flush") {
	HttpOutputConfig cfg {};
	cfg.flags.InitialEmptyPost = false;
	cfg.url = "http://127.0.0.1:9000";
	cfg.endOfSessionSuffix = { 'G', 'o', 'o', 'd', 'b', 'y', 'e'};
	auto mod = loadModule("HTTP", &NullHost, &cfg);
	mod->getInput(0)->push(createPacket("ThisIsPostData"));
	mod->process();
}

