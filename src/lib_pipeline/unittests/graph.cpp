#include "tests/tests.hpp"
#include "lib_pipeline/pipeline.hpp"
#include "lib_modules/utils/helper.hpp"
#include <sstream>

using namespace Modules;
using namespace Pipelines;
using namespace std;

namespace {

struct Dummy : public Module {
	Dummy(IModuleHost*) {
		addInput(this);
		output = addOutput<OutputDefault>();
	}
	void process() {
		output->emit(output->getBuffer(1));
	}
	OutputDefault* output;
};

string replaceAll(string haystack, string needle, string with) {
	auto pos = haystack.find(needle);

	while( pos != std::string::npos) {
		haystack.replace(pos, needle.size(), with);
		pos = haystack.find(needle, pos + needle.size());
	}

	return haystack;
}

string formatPtr(void* p) {
	stringstream ss;
	ss << p;
	return ss.str();
}

unittest("pipeline graph: empty") {
	Pipeline p;
	string expected =
	    R"(digraph {
	rankdir = "LR";
}
)";
	ASSERT_EQUALS(expected, p.dump());
}

unittest("pipeline graph: add module") {
	Pipeline p;
	auto A = p.addModule<Dummy>();
	string expected =
	    R"(digraph {
	rankdir = "LR";
	"0";
}
)";

	auto str = replaceAll(p.dump(), formatPtr(A), "A");
	ASSERT_EQUALS(expected, str);
}

unittest("pipeline graph: add connection") {
	Pipeline p;
	auto A = p.addModule<Dummy>();
	auto B = p.addModule<Dummy>();
	p.connect(A, B);
	string expected =
	    R"(digraph {
	rankdir = "LR";
	"0";
	"1";
	"0" -> "1";
}
)";

	auto str = p.dump();
	str = replaceAll(str, formatPtr(A), "A");
	str = replaceAll(str, formatPtr(B), "B");
	ASSERT_EQUALS(expected, str);
}

unittest("pipeline graph: disconnect") {
	Pipeline p;
	auto A = p.addModule<Dummy>();
	auto B = p.addModule<Dummy>();
	p.connect(A, B);
	p.disconnect(A, 0, B, 0);
	string expected =
	    R"(digraph {
	rankdir = "LR";
	"0";
	"1";
}
)";

	auto str = p.dump();
	str = replaceAll(str, formatPtr(A), "A");
	str = replaceAll(str, formatPtr(B), "B");
	ASSERT_EQUALS(expected, str);
}

unittest("pipeline graph: remove module") {
	Pipeline p;
	auto ptr = p.addModule<Dummy>();
	p.removeModule(ptr);
	string expected =
	    R"(digraph {
	rankdir = "LR";
}
)";
	ASSERT_EQUALS(expected, p.dump());
}

unittest("pipeline graph: remove wrong connection") {
	Pipeline p;
	auto ptr1 = p.addModule<Dummy>();
	auto ptr2 = p.addModule<Dummy>();
	ASSERT_THROWN(p.disconnect(ptr1, 0, ptr2, 0));
}

unittest("pipeline graph: remove module still connected") {
	Pipeline p;
	auto ptr1 = p.addModule<Dummy>();
	auto ptr2 = p.addModule<Dummy>();
	p.connect(ptr1, ptr2);
	ASSERT_THROWN(p.removeModule(ptr1));
	ASSERT_THROWN(p.removeModule(ptr2));
}

}
