#include "http_input.hpp"
#include "lib_modules/utils/factory.hpp" // registerModule
#include "lib_modules/utils/helper.hpp"
#include "lib_utils/tools.hpp" // enforce
#include "lib_media/common/file_puller.hpp"

using namespace Modules;

std::unique_ptr<In::IFilePuller> createHttpSource();

namespace {

struct HttpInput : Module {
		HttpInput(KHost *host, HttpInputConfig const& cfg) : url(cfg.url) {
			addOutput();
			host->activate(true);
		}
		void process() override {
			if (!source) {
				auto onBuffer = [&](SpanC chunk) {
					auto data = std::make_shared<DataRaw>(chunk.len);
					memcpy(data->buffer->data().ptr, chunk.ptr, chunk.len);
					outputs[0]->post(data);
				};

				source = createHttpSource();
				source->wget(url.c_str(), onBuffer);
			}
		}

	private:
		std::string url;
		std::unique_ptr<In::IFilePuller> source;
};

IModule* createObject(KHost* host, void* va) {
	auto config = (HttpInputConfig*)va;
	enforce(host, "HttpInput: host can't be NULL");
	enforce(config, "HttpInput: config can't be NULL");
	return createModule<HttpInput>(host, *config).release();
}

auto const registered = Factory::registerModule("HttpInput", &createObject);

}
