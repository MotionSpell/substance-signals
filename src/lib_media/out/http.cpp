#include "http.hpp"
#include "lib_modules/utils/factory.hpp" // registerModule
#include "lib_utils/log_sink.hpp" // Warning
#include "lib_utils/format.hpp"
#include <string.h> // memcpy
#include <thread>

extern "C" {
#include <curl/curl.h>
}

using namespace Modules;

namespace Modules {
namespace Out {

namespace {
size_t writeVoid(void *buffer, size_t size, size_t nmemb, void *userp) {
	(void)buffer;
	(void)userp;
	return size * nmemb;
}

size_t read(span<const uint8_t>& stream, uint8_t* dst, size_t dstLen) {
	auto readCount = std::min(stream.len, dstLen);
	memcpy(dst, stream.ptr, readCount);
	stream += readCount;
	return readCount;
}

CURL* createCurl() {
	auto curl = curl_easy_init();
	if (!curl)
		throw std::runtime_error("Couldn't init the HTTP stack.");

	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	if(0)
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

	// trash all the replies from the server
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeVoid);

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	return curl;
}

void enforceConnection(std::string url, bool usePUT) {
	std::shared_ptr<void> curlPointer(createCurl(), &curl_easy_cleanup);
	auto curl = curlPointer.get();

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	if (usePUT) {
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
		curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, 0);
	} else {
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0);
	}
	auto const res = curl_easy_perform(curl);
	if (res != CURLE_OK)
		throw std::runtime_error(format("curl_easy_perform() failed for URL: %s", url));
}

}

struct HTTP::Private {

	Private() {
		curl_global_init(CURL_GLOBAL_ALL);
		curl = createCurl();
	}

	~Private() {
		curl_slist_free_all(chunk);
		curl_easy_cleanup(curl);
		curl_global_cleanup();
	}
	struct curl_slist *chunk = nullptr;
	CURL *curl;
	std::thread workingThread;
};

HTTP::HTTP(IModuleHost* host, HttpOutputConfig const& cfg)
	: m_host(host), url(cfg.url), userAgent(cfg.userAgent), flags(cfg.flags) {
	if (url.compare(0, 7, "http://") && url.compare(0, 8, "https://"))
		throw error(format("can only handle URLs starting with 'http://' or 'https://', not '%s'.", url));

	m_pImpl = make_unique<Private>();

	auto& curl = m_pImpl->curl;

	//make an empty POST to check the end point exists
	if (flags.InitialEmptyPost)
		enforceConnection(url, flags.UsePUT);

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
	if (flags.UsePUT) {
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	} else {
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
	}

	curl_easy_setopt(curl, CURLOPT_READFUNCTION, &HTTP::staticCurlCallback);
	curl_easy_setopt(curl, CURLOPT_READDATA, this);

	if (flags.Chunked) {
		m_pImpl->chunk = curl_slist_append(m_pImpl->chunk, "Transfer-Encoding: chunked");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, m_pImpl->chunk);
	}
	for (auto &h : cfg.headers) {
		m_pImpl->chunk = curl_slist_append(m_pImpl->chunk, h.c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, m_pImpl->chunk);
	}

	addInput(this);
	outputFinished = addOutput<OutputDefault>();
}

HTTP::~HTTP() {
	endOfStream();
}

void HTTP::endOfStream() {
	if (m_pImpl->workingThread.joinable()) {
		inputs[0]->push(nullptr);
		m_pImpl->workingThread.join();
	}
}

void HTTP::flush() {
	endOfStream();
}

void HTTP::process() {
	if (!m_pImpl->workingThread.joinable() && state == Init) {
		state = RunNewConnection;
		m_pImpl->workingThread = std::thread(&HTTP::threadProc, this);
	}
}

void HTTP::readTransferedBs(uint8_t* dst, size_t size) {
	auto n = read(m_currBs, dst, size);
	if (n != size)
		throw error("Short read on transfered bitstream");
}

bool HTTP::open() {
	assert(!m_currBs.ptr);
	m_currBs = m_currData->data();
	return true;
}

void HTTP::clean() {
	m_currBs = {};
	m_currData = nullptr;
}

size_t HTTP::staticCurlCallback(void *buffer, size_t size, size_t nmemb, void *userp) {
	auto pThis = (HTTP*)userp;
	return pThis->fillBuffer(span<uint8_t>((uint8_t*)buffer, size * nmemb));
}

size_t HTTP::fillBuffer(span<uint8_t> buffer) {
	if (state == RunNewConnection && m_currData) {
		if (m_currBs.ptr) {
			auto meta = safe_cast<const MetadataFile>(m_currData->getMetadata());
			m_host->log(Warning, format("Reconnect: file %s", meta->filename).c_str());
			m_currBs = m_currData->data();
		} else { /*we may be exiting because of an exception*/
			m_currData = nullptr;
			inputs[0]->push(nullptr);
		}
	}

	if (!m_currData) {
		m_currData = inputs[0]->pop();
		if (!m_currData) {
			auto out = outputFinished->getBuffer(0);
			out->setMetadata(m_currMetadata);
			outputFinished->emit(out);

			if (state == Stop)
				return 0;

			state = Stop;
			auto n = endOfSession(buffer);
			if (n) inputs[0]->push(nullptr);
			return n;
		}

		m_currMetadata = m_currData->getMetadata();
		if (!open()) {
			return 0;
		}
		if (state != RunNewConnection) {
			state = RunNewFile; //on new connection, don't call newFileCallback()
		}
	}

	if (state == RunNewConnection) {
		state = RunResume;
	} else if (state == RunNewFile) {
		newFileCallback(buffer);
		state = RunResume;
	}

	auto const desiredCount = std::min(m_currBs.len, buffer.len);
	auto const readCount = read(m_currBs, buffer.ptr, desiredCount);
	if (readCount == 0) {
		clean();
		return fillBuffer(buffer);
	}

	return readCount;
}

bool HTTP::performTransfer() {
	CURLcode res = curl_easy_perform(m_pImpl->curl);
	if (res != CURLE_OK) {
		m_host->log(Warning, format("curl_easy_perform() failed for URL %s: %s", url, curl_easy_strerror(res)).c_str());
	}

	if (state == Stop) {
		return false;
	} else {
		state = RunNewConnection;
		return true;
	}
}

void HTTP::threadProc() {
	if (flags.Chunked) {
		while (state != Stop) {
			if (!performTransfer()) {
				break;
			}
		}
	} else {
		performTransfer();
	}
}

}
}

namespace {

IModule* createObject(IModuleHost* host, va_list va) {
	auto cfg = va_arg(va, HttpOutputConfig*);
	enforce(host, "HTTP: host can't be NULL");
	return create<Out::HTTP>(host, *cfg).release();
}

auto const registered = Factory::registerModule("HTTP", &createObject);
}

