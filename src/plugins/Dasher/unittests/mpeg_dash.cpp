#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_media/common/attributes.hpp"
#include "lib_media/common/metadata.hpp"
#include "plugins/Dasher/mpeg_dash.hpp" // DasherConfig

using namespace Tests;
using namespace Modules;
using namespace std;

unittest("dasher: simple") {

	struct MyOutput : ModuleS {
		void processOne(Data) override {
			++frameCount;
		}
		int frameCount = 0;
	};

	DasherConfig cfg {};
	cfg.segDurationInMs = 3000;
	auto dasher = loadModule("MPEG_DASH", &NullHost, &cfg);
	auto rec = createModule<MyOutput>();
	ConnectOutputToInput(dasher->getOutput(0), rec->getInput(0));

	dasher->flush();

	ASSERT_EQUALS(0, rec->frameCount);
}


