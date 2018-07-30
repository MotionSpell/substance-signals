#include "tests/tests.hpp"
#include "lib_pipeline/pipeline.hpp"
#include "lib_modules/utils/helper.hpp"
#include <sstream>

using namespace Modules;
using namespace Pipelines;

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

unittest("pipeline graph: empty") {
	Pipeline p;
	auto str = p.dump();
	std::stringstream expected;
	expected << "digraph {\n}\n";
	ASSERT_EQUALS(expected.str(), str);
}

unittest("pipeline graph: add module") {
	Pipeline p;
	auto ptr = p.addModule<Dummy>();
	auto str = p.dump();
	std::stringstream expected;
	expected << "digraph {\n"
	    << "\tsubgraph cluster_0 {\n"
	    << "\t\tlabel = \"" << ptr << "\";\n"
	    << "\t\tsubgraph cluster_inputs {\n"
	    << "\t\t\tlabel = \"inputs\";\n"
	    << "\t\t\t\"" << ptr << "_input_0\";\n"
	    << "\t\t}\n"
	    << "\t\tsubgraph cluster_outputs {\n"
	    << "\t\t\tlabel = \"outputs\";\n"
	    << "\t\t\t\"" << ptr << "_output_0\";\n"
	    << "\t\t}\n"
	    << "\t}\n\n"
	    << "}\n";
	ASSERT_EQUALS(expected.str(), str);
}

unittest("pipeline graph: add connection") {
	Pipeline p;
	auto ptr1 = p.addModule<Dummy>();
	auto ptr2 = p.addModule<Dummy>();
	p.connect(ptr1, 0, ptr2, 0);
	auto str = p.dump();
	std::stringstream expected;
	expected << "digraph {\n"
	    << "\tsubgraph cluster_0 {\n"
	    << "\t\tlabel = \"" << ptr1 << "\";\n"
	    << "\t\tsubgraph cluster_inputs {\n"
	    << "\t\t\tlabel = \"inputs\";\n"
	    << "\t\t\t\"" << ptr1 << "_input_0\";\n"
	    << "\t\t}\n"
	    << "\t\tsubgraph cluster_outputs {\n"
	    << "\t\t\tlabel = \"outputs\";\n"
	    << "\t\t\t\"" << ptr1 << "_output_0\";\n"
	    << "\t\t}\n"
	    << "\t}\n\n"
	    << "\tsubgraph cluster_1 {\n"
	    << "\t\tlabel = \"" << ptr2 << "\";\n"
	    << "\t\tsubgraph cluster_inputs {\n"
	    << "\t\t\tlabel = \"inputs\";\n"
	    << "\t\t\t\"" << ptr2 << "_input_0\";\n"
	    << "\t\t}\n"
	    << "\t\tsubgraph cluster_outputs {\n"
	    << "\t\t\tlabel = \"outputs\";\n"
	    << "\t\t\t\"" << ptr2 << "_output_0\";\n"
	    << "\t\t}\n"
	    << "\t}\n\n"
	    << "\t\"" << ptr1 << "_output_0\" -> \"" << ptr2 << "_input_0\";\n"
	    << "}\n";
	ASSERT_EQUALS(expected.str(), str);
}

unittest("pipeline graph: disconnect") {
	Pipeline p;
	auto ptr1 = p.addModule<Dummy>();
	auto ptr2 = p.addModule<Dummy>();
	p.connect(ptr1, 0, ptr2, 0);
	p.disconnect(ptr1, 0, ptr2, 0);
	auto str = p.dump();
	std::stringstream expected;
	expected << "digraph {\n"
	    << "\tsubgraph cluster_0 {\n"
	    << "\t\tlabel = \"" << ptr1 << "\";\n"
	    << "\t\tsubgraph cluster_inputs {\n"
	    << "\t\t\tlabel = \"inputs\";\n"
	    << "\t\t\t\"" << ptr1 << "_input_0\";\n"
	    << "\t\t}\n"
	    << "\t\tsubgraph cluster_outputs {\n"
	    << "\t\t\tlabel = \"outputs\";\n"
	    << "\t\t\t\"" << ptr1 << "_output_0\";\n"
	    << "\t\t}\n"
	    << "\t}\n\n"
	    << "\tsubgraph cluster_1 {\n"
	    << "\t\tlabel = \"" << ptr2 << "\";\n"
	    << "\t\tsubgraph cluster_inputs {\n"
	    << "\t\t\tlabel = \"inputs\";\n"
	    << "\t\t\t\"" << ptr2 << "_input_0\";\n"
	    << "\t\t}\n"
	    << "\t\tsubgraph cluster_outputs {\n"
	    << "\t\t\tlabel = \"outputs\";\n"
	    << "\t\t\t\"" << ptr2 << "_output_0\";\n"
	    << "\t\t}\n"
	    << "\t}\n\n"
	    << "}\n";
	ASSERT_EQUALS(expected.str(), str);
}

unittest("pipeline graph: remove module") {
	Pipeline p;
	auto ptr = p.addModule<Dummy>();
	p.removeModule(ptr);
	auto str = p.dump();
	std::stringstream expected;
	expected << "digraph {\n}\n";
	ASSERT_EQUALS(expected.str(), str);
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
