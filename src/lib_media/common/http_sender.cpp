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

CURL* createCurl(std::string url, bool usePUT) {
	auto curl = curl_easy_init();
	if (!curl)
		throw std::runtime_error("Couldn't init the HTTP stack.");

	// setup HTTP request
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	if (usePUT)
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	else
		curl_easy_setopt(curl, CURLOPT_POST, 1L);

	// don't check certifcates
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	// keep execution silent
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeVoid);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

	return curl;
}

Data createData(std::vector<uint8_t> const& contents) {
	auto r = make_shared<DataRaw>(contents.size());
	if(contents.size())
		memcpy(r->data().ptr, contents.data(), contents.size());
	return r;
}
}

struct CurlHttpSender : HttpSender {

		CurlHttpSender(std::string url, std::string userAgent, bool usePUT, std::vector<std::string> extraHeaders, Modules::KHost* log) {
			m_log = log;
			curl_global_init(CURL_GLOBAL_ALL);
			curl = createCurl(url, usePUT);

			curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());

			for (auto &h : extraHeaders) {
				headers = curl_slist_append(headers, h.c_str());
				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
			}

			workingThread = std::thread(&CurlHttpSender::threadProc, this);
		}

		~CurlHttpSender() {
			m_fifo.push(nullptr);
			workingThread.join();

			curl_slist_free_all(headers);
			curl_easy_cleanup(curl);
			curl_global_cleanup();
		}

		void send(span<const uint8_t> prefix) override {
			auto data = createData({prefix.ptr, prefix.ptr+prefix.len});
			m_fifo.push(data);
		}

		void setPrefix(span<const uint8_t>  prefix) override {
			m_prefixData = createData({prefix.ptr, prefix.ptr+prefix.len});
		}

	private:
		void threadProc() {
			headers = curl_slist_append(headers, "Transfer-Encoding: chunked");
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

			curl_easy_setopt(curl, CURLOPT_READFUNCTION, &CurlHttpSender::staticCurlCallback);
			curl_easy_setopt(curl, CURLOPT_READDATA, this);

			do {
				finished = false;
				m_currBs = {};

				// load prefix, if any
				if(m_prefixData)
					m_currBs = m_prefixData->data();

				auto res = curl_easy_perform(curl);
				if (res != CURLE_OK)
					m_log->log(Warning, (std::string("Transfer failed: ") + curl_easy_strerror(res)).c_str());
			} while(!finished);
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

		bool finished;

		Data m_prefixData;
		Data m_currData;
		span<const uint8_t> m_currBs {}; // points to the contents of m_currData/m_prefixData

		Modules::KHost* m_log {};
		curl_slist* headers {};
		CURL *curl;
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

std::unique_ptr<HttpSender> createHttpSender(std::string url, std::string userAgent, bool usePUT, std::vector<std::string> extraHeaders, KHost* log) {
	return std::make_unique<CurlHttpSender>(url, userAgent, usePUT, extraHeaders, log);
}

void enforceConnection(std::string url, bool usePUT) {
	std::shared_ptr<void> curl(createCurl(url, usePUT), &curl_easy_cleanup);

	if (usePUT)
		curl_easy_setopt(curl.get(), CURLOPT_INFILESIZE_LARGE, 0);
	else
		curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, 0);

	auto const res = curl_easy_perform(curl.get());
	if (res != CURLE_OK)
		throw std::runtime_error("Can't connect to '" + url + "'");
}

