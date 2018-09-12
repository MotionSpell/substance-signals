#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_media/transform/telx2ttml.hpp"

using namespace Tests;
using namespace Modules;

unittest("telx2ttml: simple") {
	TeletextToTtmlConfig cfg;
	auto reader = loadModule("TeletextToTTML", &NullHost, &cfg);
}

