#include "tests/tests.hpp"
#include "lib_pipeline/pipeline.hpp"
#include "lib_modules/utils/helper.hpp"
#include <sstream>

using namespace Modules;
using namespace Pipelines;

namespace {

struct Dummy : public Module {
	Dummy() {
		addInput(new Input<DataBase>(this));
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
	expected << "digraph {" << std::endl << "}" << std::endl;
	ASSERT_EQUALS(expected.str(), str);
}

unittest("pipeline graph: add module") {
	Pipeline p;
	auto ptr = p.addModule<Dummy>();
	auto str = p.dump();
	std::stringstream expected;
	expected << "digraph {" << std::endl
	    << "\tsubgraph cluster_0 {" << std::endl
	    << "\t\tlabel = \"" << ptr << "\";" << std::endl
	    << "\t\tsubgraph cluster_inputs {" << std::endl
	    << "\t\t\tlabel = \"inputs\";" << std::endl
	    << "\t\t\t\"" << ptr << "_input_0\";" << std::endl
	    << "\t\t}" << std::endl
	    << "\t\tsubgraph cluster_outputs {" << std::endl
	    << "\t\t\tlabel = \"outputs\";" << std::endl
	    << "\t\t\t\"" << ptr << "_output_0\";" << std::endl
	    << "\t\t}" << std::endl
	    << "\t}" << std::endl << std::endl
	    << "}" << std::endl;
	ASSERT_EQUALS(expected.str(), str);
}

unittest("pipeline graph: add connection") {
	Pipeline p;
	auto ptr1 = p.addModule<Dummy>();
	auto ptr2 = p.addModule<Dummy>();
	p.connect(ptr1, 0, ptr2, 0);
	auto str = p.dump();
	std::stringstream expected;
	expected << "digraph {" << std::endl
	    << "\tsubgraph cluster_0 {" << std::endl
	    << "\t\tlabel = \"" << ptr1 << "\";" << std::endl
	    << "\t\tsubgraph cluster_inputs {" << std::endl
	    << "\t\t\tlabel = \"inputs\";" << std::endl
	    << "\t\t\t\"" << ptr1 << "_input_0\";" << std::endl
	    << "\t\t}" << std::endl
	    << "\t\tsubgraph cluster_outputs {" << std::endl
	    << "\t\t\tlabel = \"outputs\";" << std::endl
	    << "\t\t\t\"" << ptr1 << "_output_0\";" << std::endl
	    << "\t\t}" << std::endl
	    << "\t}" << std::endl << std::endl
	    << "\tsubgraph cluster_1 {" << std::endl
	    << "\t\tlabel = \"" << ptr2 << "\";" << std::endl
	    << "\t\tsubgraph cluster_inputs {" << std::endl
	    << "\t\t\tlabel = \"inputs\";" << std::endl
	    << "\t\t\t\"" << ptr2 << "_input_0\";" << std::endl
	    << "\t\t}" << std::endl
	    << "\t\tsubgraph cluster_outputs {" << std::endl
	    << "\t\t\tlabel = \"outputs\";" << std::endl
	    << "\t\t\t\"" << ptr2 << "_output_0\";" << std::endl
	    << "\t\t}" << std::endl
	    << "\t}" << std::endl << std::endl
	    << "\t\"" << ptr1 << "_output_0\" -> \"" << ptr2 << "_input_0\";" << std::endl
	    << "}" << std::endl;
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
	expected << "digraph {" << std::endl
	    << "\tsubgraph cluster_0 {" << std::endl
	    << "\t\tlabel = \"" << ptr1 << "\";" << std::endl
	    << "\t\tsubgraph cluster_inputs {" << std::endl
	    << "\t\t\tlabel = \"inputs\";" << std::endl
	    << "\t\t\t\"" << ptr1 << "_input_0\";" << std::endl
	    << "\t\t}" << std::endl
	    << "\t\tsubgraph cluster_outputs {" << std::endl
	    << "\t\t\tlabel = \"outputs\";" << std::endl
	    << "\t\t\t\"" << ptr1 << "_output_0\";" << std::endl
	    << "\t\t}" << std::endl
	    << "\t}" << std::endl << std::endl
	    << "\tsubgraph cluster_1 {" << std::endl
	    << "\t\tlabel = \"" << ptr2 << "\";" << std::endl
	    << "\t\tsubgraph cluster_inputs {" << std::endl
	    << "\t\t\tlabel = \"inputs\";" << std::endl
	    << "\t\t\t\"" << ptr2 << "_input_0\";" << std::endl
	    << "\t\t}" << std::endl
	    << "\t\tsubgraph cluster_outputs {" << std::endl
	    << "\t\t\tlabel = \"outputs\";" << std::endl
	    << "\t\t\t\"" << ptr2 << "_output_0\";" << std::endl
	    << "\t\t}" << std::endl
	    << "\t}" << std::endl << std::endl
	    << "}" << std::endl;
	ASSERT_EQUALS(expected.str(), str);
}

unittest("pipeline graph: remove module") {
	Pipeline p;
	auto ptr = p.addModule<Dummy>();
	p.removeModule(ptr);
	auto str = p.dump();
	std::stringstream expected;
	expected << "digraph {" << std::endl << "}" << std::endl;
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
