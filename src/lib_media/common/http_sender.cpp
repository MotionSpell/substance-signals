#include "http_sender.hpp"
#include <string.h> // memcpy
#include <thread>
#include "lib_utils/log_sink.hpp" // Warning
#include "lib_utils/queue.hpp"

extern "C" {
#include <curl/curl.h>
}

using namespace Modules;

namespace {
size_t writeVoid(void*, size_t size, size_t nmemb, void*) {
	return size * nmemb;
}

struct CurlScope {
	CurlScope() {
		curl_global_init(CURL_GLOBAL_ALL);
	}
	~CurlScope() {
		curl_global_cleanup();
	}
};

std::shared_ptr<CURL> createCurl(std::string url, bool usePUT) {
	auto curl = std::shared_ptr<CURL>(curl_easy_init(), &curl_easy_cleanup);
	if (!curl)
		throw std::runtime_error("Couldn't init the HTTP stack.");

	// setup HTTP request
	curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
	if (usePUT)
		curl_easy_setopt(curl.get(), CURLOPT_UPLOAD, 1L);
	else
		curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);

	// don't check certifcates
	curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 0L);

	// keep execution silent
	curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, writeVoid);
	curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl.get(), CURLOPT_VERBOSE, 0L);

	return curl;
}

Data createData(SpanC contents) {
	auto r = make_shared<DataRaw>(contents.len);
	if(contents.len)
		memcpy(r->data().ptr, contents.ptr, contents.len);
	return r;
}
}

struct CurlHttpSender : HttpSender {

		HttpSenderConfig const m_cfg;
		CurlHttpSender(HttpSenderConfig const& cfg, Modules::KHost* log) : m_cfg(cfg) {
			m_log = log;
			workingThread = std::thread(&CurlHttpSender::threadProc, this);
		}

		~CurlHttpSender() {

			// Allows the working thread to exit, in case curl_easy_perform
			// does not call our callback. This occurs when the webserver returns
			// a "403 Forbidden" error.
			finished = true;

			m_fifo.clear();
			m_fifo.push(nullptr);
			workingThread.join();
		}

		void send(span<const uint8_t> prefix) override {
			auto data = createData(prefix);
			m_fifo.push(data);
		}

		void setPrefix(span<const uint8_t>  prefix) override {
			m_prefixData = createData(prefix);
		}

	private:
		void threadProc() {

			auto curl = createCurl(m_cfg.url, m_cfg.usePUT);

			curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, m_cfg.userAgent.c_str());

			for (auto &h : m_cfg.extraHeaders) {
				headers = curl_slist_append(headers, h.c_str());
				curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
			}

			headers = curl_slist_append(headers, "Transfer-Encoding: chunked");
			curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);

			curl_easy_setopt(curl.get(), CURLOPT_READFUNCTION, &CurlHttpSender::staticCurlCallback);
			curl_easy_setopt(curl.get(), CURLOPT_READDATA, this);

			while(!finished) {
				m_currBs = {};

				// load prefix, if any
				if(m_prefixData)
					m_currBs = m_prefixData->data();

				auto res = curl_easy_perform(curl.get());
				if (res != CURLE_OK)
					m_log->log(Warning, (std::string("Transfer failed: ") + curl_easy_strerror(res)).c_str());

				long http_code = 0;
				curl_easy_getinfo (curl.get(), CURLINFO_RESPONSE_CODE, &http_code);
				if(http_code >= 400)
					m_log->log(Warning, ("HTTP error: " + std::to_string(http_code) + " (" + m_cfg.url + ")").c_str());
			}

			curl_slist_free_all(headers);
		}

		static size_t staticCurlCallback(void *buffer, size_t size, size_t nmemb, void *userp) {
			auto pThis = (CurlHttpSender*)userp;
			return pThis->fillBuffer(span<uint8_t>((uint8_t*)buffer, size * nmemb));
		}

		size_t fillBuffer(span<uint8_t> buffer) {
			// curl stops calling us when we return 0.
			if(finished)
				return 0;

			auto writer = buffer;
			while(writer.len > 0) {
				if (m_currBs.len == 0) {
					m_currData = m_fifo.pop();
					if(!m_currData) {
						finished = true;
						break;
					}
					m_currBs = m_currData->data();
				}

				writer += read(m_currBs, writer.ptr, writer.len);
			}

			return writer.ptr - buffer.ptr;
		}

		CurlScope m_curlScope;
		bool finished = false; // set to 'true' when the curl callback pops the 'null' sample (pushed by the destructor)

		Data m_prefixData;
		Data m_currData;
		span<const uint8_t> m_currBs {}; // points to the contents of m_currData/m_prefixData

		Modules::KHost* m_log {};
		curl_slist* headers {};
		std::thread workingThread;

		// data to upload
		Queue<Data> m_fifo;

		static size_t read(span<const uint8_t>& stream, uint8_t* dst, size_t dstLen) {
			auto readCount = std::min(stream.len, dstLen);
			if(readCount > 0)
				memcpy(dst, stream.ptr, readCount);
			stream += readCount;
			return readCount;
		}
};

std::unique_ptr<HttpSender> createHttpSender(HttpSenderConfig const& config, Modules::KHost* log) {
	return std::make_unique<CurlHttpSender>(config, log);
}

void enforceConnection(std::string url, bool usePUT) {
	CurlScope curlScope;

	auto curl = createCurl(url, usePUT);

	if (usePUT)
		curl_easy_setopt(curl.get(), CURLOPT_INFILESIZE_LARGE, 0);
	else
		curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, 0);

	auto const res = curl_easy_perform(curl.get());
	if (res != CURLE_OK)
		throw std::runtime_error("Can't connect to '" + url + "'");
}

