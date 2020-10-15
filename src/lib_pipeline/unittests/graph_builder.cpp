#include "tests/tests.hpp"
#include "lib_pipeline/pipeline.hpp"
#include "lib_pipeline/graph_builder.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_modules/utils/factory.hpp"

using namespace std;
using namespace Pipelines;


namespace Modules {

struct Dummy : public Module {
	Dummy(KHost*) {
		addInput();
		addOutput();
	}
	void process() override {
	}
};

IModule* createObject(KHost* host, void* /*va*/) {
	return createModule<Dummy>(host).release();
}

auto const registered = Factory::registerModule("Dummy", &createObject);
}

unittest("graph builder: dummy") {
	string json =
	    R"|({
    "version" : 1,
    "modules" : {
        "dummy0" : {
            "type" : "Dummy",
            "config" : []
        },
        "dummy1" : {
            "type" : "Dummy",
            "config" : []
        }
    },
    "connections" : [
        {
            "dummy0": 0,
            "dummy1": 0
        }
    ]
}
)|";

string expected = R"|(digraph {
	rankdir = "LR";
	"Dummy (#0)";
	"Dummy (#1)";
	"Dummy (#0)" -> "Dummy (#1)";
}
)|";

auto p = createPipelineFromJSON(json);
ASSERT_EQUALS(expected, p->dumpDOT());
}
