#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/transform/telx2ttml.hpp"

using namespace Tests;
using namespace Modules;
using namespace Transform;

unittest("telx2ttml: simple") {
	TeletextToTtmlConfig cfg;
	auto reader = create<TeletextToTTML>(&NullHost, &cfg);
}

