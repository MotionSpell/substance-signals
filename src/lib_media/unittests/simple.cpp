#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include <iostream>
#include "lib_media/in/file.hpp"
#include "lib_media/out/print.hpp"
#include "lib_utils/tools.hpp"

using namespace Tests;
using namespace Modules;

unittest("empty param test: File") {
	ASSERT_THROWN(createModule<In::File>(&NullHost, ""));
}

unittest("empty param test: Out::Print") {
	auto p = createModule<Out::Print>(&NullHost, std::cout);
}

unittest("simple param test") {
	auto f = createModule<In::File>(&NullHost, "data/beepbop.mp4");
}

unittest("print packets size from file: File -> Out::Print") {
	auto f = createModule<In::File>(&NullHost, "data/beepbop.mp4");
	auto p = createModule<Out::Print>(&NullHost, std::cout);

	ConnectOutputToInput(f->getOutput(0), p->getInput(0));

	f->process();
}
