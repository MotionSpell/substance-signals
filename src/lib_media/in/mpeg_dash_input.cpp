#include "lib_utils/tools.hpp"
#include "mpeg_dash_input.hpp"

#define IOSIZE (64*1024)

using namespace Modules::In;

namespace Modules {
namespace In {

MPEG_DASH_Input::MPEG_DASH_Input(IHttpSource* httpSource, std::string const& url) {
	//GET MPD FROM HTTP
	auto mpd = httpSource->get(url);

	//PARSE MPD

	//DECLARE OUTPUT PORTS
	for(int i=0; i < 2; ++i)
		outputs.push_back(nullptr);
}

MPEG_DASH_Input::~MPEG_DASH_Input() {
}

void MPEG_DASH_Input::process(Data data) {
	for (;;) {
		if (getNumInputs() && getInput(0)->tryPop(data))
			break;

#if 0
		auto out = output->getBuffer(IOSIZE);
		size_t read = fread(out->data(), 1, IOSIZE, file);
		if (read < IOSIZE) {
			if (read == 0) {
				break;
			}
			out->resize(read);
		}
		output->emit(out);
#endif
	}
}

}
}


///////////////////////////////////////////////////////////////////////////////
// move this elsewhere

extern "C" {
#include <curl/curl.h>
}

struct HttpSource : IHttpSource {

	std::string get(std::string url) override {

		struct HttpContext {
			std::vector<uint8_t> data;

			static size_t callback(void *stream, size_t size, size_t nmemb, void *ptr) {
				auto pThis = (HttpContext*)ptr;
        auto const bytes = size * nmemb;
				pThis->onReceiveBuffer(bytes, (uint8_t*)stream);
        return bytes;
			}

			void onReceiveBuffer(size_t size, uint8_t *stream) {
				for(size_t i=0; i < size; ++i)
					data.push_back(stream[i]);
			}
		};

		auto curl = curl_easy_init();
		if(!curl)
			throw std::runtime_error("can't init curl");

		HttpContext ctx;

		// some servers require a user-agent field
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &HttpContext::callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

		auto res = curl_easy_perform(curl);
		if(res != CURLE_OK)
			throw std::runtime_error(std::string("curl_easy_perform() failed: ") + curl_easy_strerror(res));

		curl_easy_cleanup(curl);
		return std::string(ctx.data.begin(), ctx.data.end());
	}
};

std::unique_ptr<IHttpSource> createHttpSource() {
	return uptr(new HttpSource);
}


