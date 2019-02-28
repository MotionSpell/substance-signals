#include "http_sink.hpp"

#include "lib_modules/utils/helper.hpp" // ModuleS
#include "lib_modules/utils/factory.hpp"
#include "lib_utils/format.hpp"
#include "lib_utils/log_sink.hpp"
#include "../common/metadata_file.hpp"
#include "http.hpp"
#include <string>
#include <memory>
#include <map>
#include <thread>
#include <atomic>

using namespace std;

template<typename T, typename V>
bool exists(T const& container, V const& val) {
	return container.find(val) != container.end();
}

struct HttpSink : Modules::ModuleS {
		//Romain: connect to the DASHer delete
		HttpSink(Modules::KHost* host, string baseURL, string userAgent, const vector<string> &headers)
			: m_host(host),
			  baseURL(baseURL), userAgent(userAgent), headers(headers) {
			done = false;
		}
		~HttpSink() {
			done = true;
		}
		void processOne(Modules::Data data) override {
			auto const meta = safe_cast<const Modules::MetadataFile>(data->getMetadata());

			for(auto url : toErase) {
				m_host->log(Debug, format("Closing HTTP connection for url: \"%s\"", url).c_str());
				zeroSizeConnections.erase(url);
			}
			toErase.clear();

			auto const url = baseURL + meta->filename;

			HttpOutputConfig httpConfig {};
			httpConfig.flags.UsePUT = false;
			httpConfig.url = url;
			httpConfig.userAgent = userAgent;
			httpConfig.headers = headers;

			if (meta->filesize == 0 && !meta->EOS) {
				if (exists(zeroSizeConnections, url))
					throw std::runtime_error(format("Received zero-sized metadata but transfer is already initialized for URL: \"%s\"", url));

				m_host->log(Info, format("Initialize transfer for URL: \"%s\"", url).c_str());
				http = Modules::createModule<Modules::Out::HTTP>(&Modules::NullHost, httpConfig);
				auto onFinished = [&](Modules::Data data2) {
					auto const url2 = baseURL + safe_cast<const Modules::MetadataFile>(data2->getMetadata())->filename;
					m_host->log(Debug, format("Finished transfer for url: \"%s\" (done=%s)", url2, (bool)done).c_str());
					if (!done) {
						toErase.push_back(url2);
						asyncRemoteDelete(url2);
					}
				};
				ConnectOutput(http->getOutput(0), onFinished);

				http->getInput(0)->push(data);
				processModule(http.get());
				zeroSizeConnections[url] = move(http);
			} else {
				if (exists(zeroSizeConnections, url)) {
					m_host->log(Debug, format("Continue transfer (%s bytes) for URL: \"%s\"", meta->filesize, url).c_str());
					if (meta->filesize) {
						zeroSizeConnections[url]->getInput(0)->push(data);
					}
					if (meta->EOS) {
						zeroSizeConnections[url]->getInput(0)->push(nullptr);
					}
					processModule(zeroSizeConnections[url].get());
				} else {
					m_host->log(Debug, format("Pushing (%s bytes) to new URL: \"%s\"", meta->filesize, url).c_str());
					http = Modules::createModule<Modules::Out::HTTP>(&Modules::NullHost, httpConfig);
					http->getInput(0)->push(data);
					auto th = thread([](unique_ptr<Modules::Out::HTTP> http) {
						http->getInput(0)->push(nullptr);
						processModule(http.get());
					}, move(http));
					th.detach();
				}
			}
		}

	private:

		void asyncRemoteDelete(string url2) {
			auto remoteDelete = [url2,this]() {
				auto cmd = format("curl -X DELETE %s", url2);
				if(system(cmd.c_str()) != 0) {
					m_host->log(Warning, format("command %s failed", cmd).c_str());
				}
			};
			auto th = thread(remoteDelete);
			th.detach();
		}

		// workaround the fact that 'ModuleS' hides the 'process(Data)' function
		static void processModule(IModule* mod) {
			mod->process();
		}

		Modules::KHost* const m_host;
		atomic_bool done;
		unique_ptr<Modules::Out::HTTP> http;
		map<string, shared_ptr<Modules::Out::HTTP>> zeroSizeConnections;
		vector<string> toErase;
		const string baseURL, userAgent;
		const vector<string> headers;
};

namespace {

using namespace Modules;

Modules::IModule* createObject(KHost* host, void* va) {
	auto config = (HttpSinkConfig*)va;
	enforce(host, "HttpSink: host can't be NULL");
	enforce(config, "HttpSink: config can't be NULL");
	return Modules::createModule<HttpSink>(host, config->baseURL, config->userAgent, config->headers).release();
}

auto const registered = Factory::registerModule("HttpSink", &createObject);
}
