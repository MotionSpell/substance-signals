#include "tests/tests.hpp"
#include "lib_pipeline/pipeline.hpp"
#include "lib_modules/utils/helper.hpp"
#include <sstream>

using namespace Modules;
using namespace Pipelines;
using namespace std;

namespace {

struct Dummy : public Module {
	Dummy() {
		addInput(new Input(this));
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
	string expected = "digraph {\n}\n";
	ASSERT_EQUALS(expected, p.dump());
}

unittest("pipeline graph: add module") {
	Pipeline p;
	auto A = p.addModule<Dummy>();
	string expected =
	    "digraph {\n"
	    "\tsubgraph cluster_0 {\n"
	    "\t\tlabel = \"A\";\n"
	    "\t\tsubgraph cluster_inputs {\n"
	    "\t\t\tlabel = \"inputs\";\n"
	    "\t\t\t\"A_input_0\";\n"
	    "\t\t}\n"
	    "\t\tsubgraph cluster_outputs {\n"
	    "\t\t\tlabel = \"outputs\";\n"
	    "\t\t\t\"A_output_0\";\n"
	    "\t\t}\n"
	    "\t}\n\n"
	    "}\n";
	auto str = replaceAll(p.dump(), formatPtr(A), "A");
	ASSERT_EQUALS(expected, str);
}

unittest("pipeline graph: add connection") {
	Pipeline p;
	auto A = p.addModule<Dummy>();
	auto B = p.addModule<Dummy>();
	p.connect(A, 0, B, 0);
	string expected =
	    "digraph {\n"
	    "\tsubgraph cluster_0 {\n"
	    "\t\tlabel = \"A\";\n"
	    "\t\tsubgraph cluster_inputs {\n"
	    "\t\t\tlabel = \"inputs\";\n"
	    "\t\t\t\"A_input_0\";\n"
	    "\t\t}\n"
	    "\t\tsubgraph cluster_outputs {\n"
	    "\t\t\tlabel = \"outputs\";\n"
	    "\t\t\t\"A_output_0\";\n"
	    "\t\t}\n"
	    "\t}\n\n"
	    "\tsubgraph cluster_1 {\n"
	    "\t\tlabel = \"B\";\n"
	    "\t\tsubgraph cluster_inputs {\n"
	    "\t\t\tlabel = \"inputs\";\n"
	    "\t\t\t\"B_input_0\";\n"
	    "\t\t}\n"
	    "\t\tsubgraph cluster_outputs {\n"
	    "\t\t\tlabel = \"outputs\";\n"
	    "\t\t\t\"B_output_0\";\n"
	    "\t\t}\n"
	    "\t}\n\n"
	    "\t\"A_output_0\" -> \"B_input_0\";\n"
	    "}\n";
	auto str = p.dump();
	str = replaceAll(str, formatPtr(A), "A");
	str = replaceAll(str, formatPtr(B), "B");
	ASSERT_EQUALS(expected, str);
}

unittest("pipeline graph: disconnect") {
	Pipeline p;
	auto A = p.addModule<Dummy>();
	auto B = p.addModule<Dummy>();
	p.connect(A, 0, B, 0);
	p.disconnect(A, 0, B, 0);
	string expected =
	    "digraph {\n"
	    "\tsubgraph cluster_0 {\n"
	    "\t\tlabel = \"A\";\n"
	    "\t\tsubgraph cluster_inputs {\n"
	    "\t\t\tlabel = \"inputs\";\n"
	    "\t\t\t\"A_input_0\";\n"
	    "\t\t}\n"
	    "\t\tsubgraph cluster_outputs {\n"
	    "\t\t\tlabel = \"outputs\";\n"
	    "\t\t\t\"A_output_0\";\n"
	    "\t\t}\n"
	    "\t}\n\n"
	    "\tsubgraph cluster_1 {\n"
	    "\t\tlabel = \"B\";\n"
	    "\t\tsubgraph cluster_inputs {\n"
	    "\t\t\tlabel = \"inputs\";\n"
	    "\t\t\t\"B_input_0\";\n"
	    "\t\t}\n"
	    "\t\tsubgraph cluster_outputs {\n"
	    "\t\t\tlabel = \"outputs\";\n"
	    "\t\t\t\"B_output_0\";\n"
	    "\t\t}\n"
	    "\t}\n\n"
	    "}\n";
	auto str = p.dump();
	str = replaceAll(str, formatPtr(A), "A");
	str = replaceAll(str, formatPtr(B), "B");
	ASSERT_EQUALS(expected, str);
}

unittest("pipeline graph: remove module") {
	Pipeline p;
	auto ptr = p.addModule<Dummy>();
	p.removeModule(ptr);
	ASSERT_EQUALS("digraph {\n}\n", p.dump());
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
	p.connect(ptr1, 0, ptr2, 0);
	ASSERT_THROWN(p.removeModule(ptr1));
	ASSERT_THROWN(p.removeModule(ptr2));
}

}
