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

CURL* createCurl(std::string url, bool usePUT) {
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

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	if (usePUT)
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	else
		curl_easy_setopt(curl, CURLOPT_POST, 1L);

	return curl;
}

void enforceConnection(std::string url, bool usePUT) {
	std::shared_ptr<void> curl(createCurl(url, usePUT), &curl_easy_cleanup);

	if (usePUT)
		curl_easy_setopt(curl.get(), CURLOPT_INFILESIZE_LARGE, 0);
	else
		curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, 0);

	auto const res = curl_easy_perform(curl.get());
	if (res != CURLE_OK)
		throw std::runtime_error(format("Can't connect to '%s'", url));
}

}

struct HTTP::Private {

	Private(std::string url, bool usePUT) {
		curl_global_init(CURL_GLOBAL_ALL);
		curl = createCurl(url, usePUT);
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
	: m_host(host), url(cfg.url), flags(cfg.flags) {
	if (url.compare(0, 7, "http://") && url.compare(0, 8, "https://"))
		throw error(format("can only handle URLs starting with 'http://' or 'https://', not '%s'.", url));

	m_pImpl = make_unique<Private>(url, cfg.flags.UsePUT);

	auto& curl = m_pImpl->curl;

	//make an empty POST to check the end point exists
	if (flags.InitialEmptyPost)
		enforceConnection(url, flags.UsePUT);

	curl_easy_setopt(curl, CURLOPT_USERAGENT, cfg.userAgent.c_str());

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

	m_pImpl->workingThread = std::thread(&HTTP::threadProc, this);
}

HTTP::~HTTP() {
	inputs[0]->push(nullptr);
	m_pImpl->workingThread.join();
}

void HTTP::endOfStream() {
	inputs[0]->push(nullptr);
}

void HTTP::flush() {
	endOfStream();

	auto out = outputFinished->getBuffer(0);
	out->setMetadata(m_currMetadata);
	outputFinished->emit(out);
}

void HTTP::process() {
}

void HTTP::readTransferedBs(uint8_t* dst, size_t size) {
	auto n = read(m_currBs, dst, size);
	if (n != size)
		throw error("Short read on transfered bitstream");
}

void HTTP::clean() {
	m_currBs = {};
	m_currData = nullptr;
}

size_t HTTP::staticCurlCallback(void *buffer, size_t size, size_t nmemb, void *userp) {
	auto pThis = (HTTP*)userp;
	return pThis->fillBuffer(span<uint8_t>((uint8_t*)buffer, size * nmemb));
}

bool HTTP::loadNextData() {
	assert(!m_currBs.ptr);

	m_currData = inputs[0]->pop();
	if(!m_currData)
		return false;

	m_currBs = m_currData->data();
	m_currMetadata = m_currData->getMetadata();
	return true;
}

size_t HTTP::fillBuffer(span<uint8_t> buffer) {
	if (state == RunNewConnection && m_currData) {
		assert (m_currBs.ptr);
		m_host->log(Warning, "Reconnect");

		// restart transfer of the current chunk from the beginning
		m_currBs = m_currData->data();
	}

	if (!m_currData) {
		if (!loadNextData()) {
			if (state == Stop)
				return 0;

			state = Stop;
			return m_controller->endOfSession(buffer);
		}

		if (state != RunNewConnection) {
			state = RunNewFile; //on new connection, don't call newFileCallback()
		}
	}

	if (state == RunNewConnection) {
		state = RunResume;
	} else if (state == RunNewFile) {
		m_controller->newFileCallback(buffer);
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
		m_host->log(Warning, format("Transfer failed for '%s': %s", url, curl_easy_strerror(res)).c_str());
	}

	if (state == Stop)
		return false;

	state = RunNewConnection;
	return true;
}

void HTTP::threadProc() {

	state = RunNewConnection;

	if (flags.Chunked) {
		while (state != Stop && performTransfer()) {
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

