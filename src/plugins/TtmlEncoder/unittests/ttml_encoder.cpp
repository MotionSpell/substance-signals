
#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_media/common/attributes.hpp"
#include "lib_media/common/metadata.hpp"
#include "lib_media/common/subtitle.hpp"
#include "../ttml_encoder.hpp"

using namespace Tests;
using namespace Modules;

namespace {
struct OutStub : ModuleS {
	std::vector<int64_t> times;
	std::vector<std::string> ttml;
	void processOne(Data data) override {
		ttml.push_back(std::string((char*)data->data().ptr, data->data().len));
		times.push_back(data->get<PresentationTime>().time);
	}
};
}

unittest("ttml_encoder: simple") {
	TtmlEncoderConfig cfg;
	auto m = loadModule("TTMLEncoder", &NullHost, &cfg);
	Page page {0,0,std::vector<Page::Line>({{"toto"}})};
	auto data = std::make_shared<DataSubtitle>(0);
	data->set(DecodingTime{ page.showTimestamp });
	data->setMediaTime(page.showTimestamp);
	data->page = page;
	
	auto ttmlAnalyzer = createModule<OutStub>();
	ConnectOutputToInput(m->getOutput(0), ttmlAnalyzer->getInput(0));

	m->getInput(0)->push(data);

	std::vector<int64_t> expectedTimes = {};
	std::vector<std::string> expectedTtml = {};

	ASSERT_EQUALS(expectedTimes, ttmlAnalyzer->times);
	ASSERT_EQUALS(expectedTtml, ttmlAnalyzer->ttml);
}
