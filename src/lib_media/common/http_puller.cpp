#include "file_puller.hpp"

#include <stdexcept>
#include <memory>

extern "C" {
#include <curl/curl.h>
}

struct HttpSource : Modules::In::IFilePuller {

	HttpSource() : curl(curl_easy_init()) {
		if(!curl)
			throw std::runtime_error("can't init curl");
	}

	~HttpSource() {
		curl_easy_cleanup(curl);
	}

	std::vector<uint8_t> get(const char* url) override {
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

		HttpContext ctx;

		// some servers require a user-agent field
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &HttpContext::callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, true);

		auto res = curl_easy_perform(curl);
		if(res == CURLE_HTTP_RETURNED_ERROR)
			return std::vector<uint8_t>();
		if(res != CURLE_OK)
			throw std::runtime_error(std::string("curl_easy_perform() failed: ") + curl_easy_strerror(res));

		return ctx.data;
	}

	CURL* const curl;
};

std::unique_ptr<Modules::In::IFilePuller> createHttpSource() {
	return std::make_unique<HttpSource>();
}
