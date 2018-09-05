// holds the chain: [dash downloader] => ( [mp4demuxer] => [restamper] )*
#include "lib_media/demux/dash_demux.hpp"
#include "lib_media/demux/gpac_demux_mp4_full.hpp"
#include "lib_media/in/mpeg_dash_input.hpp"
#include "lib_media/out/null.hpp"
#include "lib_media/transform/restamp.hpp"
#include "lib_pipeline/pipeline.hpp"

std::unique_ptr<Modules::In::IFilePuller> createHttpSource();

namespace Modules {
namespace Demux {

using namespace In;
using namespace Transform;

struct OutStub : ModuleS {
		OutStub(IModuleHost*, OutputDefault *output) : output(output) {
			addInput(this);
		}
		void process(Data data) override {
			output->emit(data);
		}

	private:
		OutputDefault *output;
};

DashDemuxer::DashDemuxer(IModuleHost* host, std::string url)
	: m_host(host) {
	pipeline = make_unique<Pipelines::Pipeline>();
	auto downloader = pipeline->addModuleWithHost<MPEG_DASH_Input>(createHttpSource(), url);

	for (int i = 0; i < (int)downloader->getNumOutputs(); ++i)
		addStream(downloader, i);
}

void DashDemuxer::addStream(Pipelines::IPipelinedModule* downloadOutput, int outputPort) {
	// create our own output
	auto output = addOutput<OutputDefault>();
	output->setMetadata(downloadOutput->getOutputMetadata(outputPort));

	// add MP4 demuxer
	auto decap = pipeline->addModuleWithHost<GPACDemuxMP4Full>();
	pipeline->connect(GetOutputPin(downloadOutput, outputPort), decap);

	// add restamper (so the timestamps start at zero)
	auto restamp = pipeline->addModuleWithHost<Restamp>(Transform::Restamp::Reset);
	pipeline->connect(decap, restamp);

	auto stub = pipeline->addModuleWithHost<OutStub>(output);
	pipeline->connect(restamp, stub);
}

bool DashDemuxer::work() {
	pipeline->start();
	pipeline->waitForEndOfStream();
	return true;
}

}
}

///////////////////////////////////////////////////////////////////////////////

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

	std::vector<uint8_t> get(std::string url) override {
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
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
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
	return make_unique<HttpSource>();
}
