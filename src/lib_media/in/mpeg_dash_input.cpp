#include "lib_utils/tools.hpp"
#include "mpeg_dash_input.hpp"

#define IOSIZE (64*1024)

using namespace Modules::In;

struct DashMpd {
	int adaptationSetCount;
};

DashMpd parseMpd(std::string text);

namespace Modules {
namespace In {

MPEG_DASH_Input::MPEG_DASH_Input(IHttpSource* httpSource, std::string const& url) {
	//GET MPD FROM HTTP
	auto mpdAsText = httpSource->get(url);

	//PARSE MPD
	auto mpd = parseMpd(mpdAsText);

	//DECLARE OUTPUT PORTS
	for(int i=0; i < mpd.adaptationSetCount; ++i)
		addOutput<OutputDefault>();
}

void MPEG_DASH_Input::process() {
}

}
}

///////////////////////////////////////////////////////////////////////////////
// nothing above this line should depend upon gpac

void enforce(bool condition, char const* msg) {
	if(condition)
		return;
	throw std::runtime_error(msg);
}

extern "C" {
#include <gpac/xml.h>
}
DashMpd parseMpd(std::string text) {
	GF_Err err;

	struct Context {

		DashMpd* mpd;

		static
		void onNodeStartCallback(void* user, const char* name, const char* namespace_, const GF_XMLAttribute *attributes, u32 nb_attributes) {
			(void)namespace_;
			(void)attributes;
			(void)nb_attributes;
			auto pThis = (Context*)user;
			pThis->onNodeStart(name);
		}

		void onNodeStart(std::string name) {
			if(name == "AdaptationSet")
				++mpd->adaptationSetCount;
		}
	};

	DashMpd r {};
	Context ctx { &r };

	auto parser = gf_xml_sax_new(&Context::onNodeStartCallback, nullptr, nullptr, &ctx);
	enforce(parser, "XML parser creation failed");

	err = gf_xml_sax_init(parser, nullptr);
	enforce(!err, "XML parser init failed");

	err = gf_xml_sax_parse(parser, text.c_str());
	enforce(!err, "XML parsing failed");

	gf_xml_sax_del(parser);

	return  r;
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
	return make_unique<HttpSource>();
}


