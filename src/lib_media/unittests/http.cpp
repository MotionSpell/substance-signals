#include "tests/tests.hpp"
#include "lib_media/out/http.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_modules/modules.hpp"

using namespace Modules;

secondclasstest("HTTP post") {
	HttpOutputConfig cfg {};
	auto demux = loadModule("HTTP", &NullHost, &cfg);
}

