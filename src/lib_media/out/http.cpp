#include "http.hpp"
#include "lib_modules/utils/factory.hpp" // registerModule
#include "lib_utils/log_sink.hpp" // Warning
#include "lib_utils/format.hpp"

extern "C" {
#include <curl/curl.h>
#include <gpac/bitstream.h>
}

using namespace Modules;
//#define CURL_DEBUG

namespace Modules {
namespace Out {

namespace {
size_t writeVoid(void *buffer, size_t size, size_t nmemb, void *userp) {
	(void)buffer;
	(void)userp;
	return size * nmemb;
}
}

HTTP::HTTP(IModuleHost* host, HttpOutputConfig const& cfg)
	: m_host(host), url(cfg.url), userAgent(cfg.userAgent), flags(cfg.flags) {
	if (url.compare(0, 7, "http://") && url.compare(0, 8, "https://"))
		throw error(format("can only handle URLs starting with 'http://' or 'https://', not %s.", url));

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (!curl)
		throw error("Couldn't init the HTTP stack.");

	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
#ifdef CURL_DEBUG
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif

	if (flags.InitialEmptyPost) { //make an empty POST to check the end point exists
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
		if (flags.UsePUT) {
			curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
			curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, 0);
		} else {
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0);
		}
		auto const res = curl_easy_perform(curl);
		if (res != CURLE_OK)
			throw error(format("curl_easy_perform() failed for URL: %s", url));

		curl_easy_reset(curl);
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
	if (flags.UsePUT) {
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	} else {
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
	}
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeVoid);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, &HTTP::staticCurlCallback);
	curl_easy_setopt(curl, CURLOPT_READDATA, this);

	if (flags.Chunked) {
		chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	}
	for (auto &h : cfg.headers) {
		chunk = curl_slist_append(chunk, h.c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	}

	addInput(this);
	outputFinished = addOutput<OutputDefault>();
}

HTTP::~HTTP() {
	endOfStream();
	curl_slist_free_all(chunk);
	curl_easy_cleanup(curl);
	curl_global_cleanup();
}

void HTTP::endOfStream() {
	if (workingThread.joinable()) {
		inputs[0]->push(nullptr);
		workingThread.join();
	}
}

void HTTP::flush() {
	endOfStream();
}

void HTTP::process() {
	if (!workingThread.joinable() && state == Init) {
		state = RunNewConnection;
		workingThread = std::thread(&HTTP::threadProc, this);
	}
}

void HTTP::readTransferedBs(uint8_t* dst, size_t size) {
	auto n = gf_bs_read_data(curTransferedBs, (char*)dst, size);
	if (n != size)
		throw error("Short read on transfered bitstream");
}

bool HTTP::open(std::shared_ptr<const MetadataFile> meta) {
	if (!meta)
		throw error("Unknown data received on input");
	assert(!curTransferedBs && !curTransferedFile);
	auto const fn = meta->filename;
	if (fn.empty() || curTransferedData->data().len) {
		curTransferedBs = gf_bs_new((const char*)curTransferedData->data().ptr, curTransferedData->data().len, GF_BITSTREAM_READ);
	} else {
		curTransferedFile = gf_fopen(fn.c_str(), "rb");
		if (!curTransferedFile) {
			if (curTransferedData->data().len) {
				m_host->log(Error, format("File %s cannot be opened", fn).c_str());
			}
			return false;
		}
		curTransferedBs = gf_bs_from_file(curTransferedFile, GF_BITSTREAM_READ);
	}
	if (!curTransferedBs)
		throw error("Bitstream cannot be created");
	return true;
}

void HTTP::clean() {
	if (curTransferedBs) {
		if (curTransferedFile) {
			gf_fclose(curTransferedFile);
			curTransferedFile = nullptr;
		}
		gf_bs_del(curTransferedBs);
		curTransferedBs = nullptr;
		curTransferedData = nullptr;
	}
}

size_t HTTP::staticCurlCallback(void *buffer, size_t size, size_t nmemb, void *userp) {
	auto pThis = (HTTP*)userp;
	return pThis->fillBuffer(span<uint8_t>((uint8_t*)buffer, size * nmemb));
}

size_t HTTP::fillBuffer(span<uint8_t> buffer) {
	if (state == RunNewConnection && curTransferedData) {
		if (curTransferedBs) {
			auto meta = safe_cast<const MetadataFile>(curTransferedData->getMetadata());
			m_host->log(Warning, format("Reconnect: file %s", meta->filename).c_str());
			gf_bs_seek(curTransferedBs, 0);
		} else { /*we may be exiting because of an exception*/
			curTransferedData = nullptr;
			inputs[0]->push(nullptr);
		}
	}

	if (!curTransferedData) {
		curTransferedData = inputs[0]->pop();
		if (!curTransferedData) {
			auto out = outputFinished->getBuffer(0);
			out->setMetadata(curTransferedMeta);
			outputFinished->emit(out);

			if (state != Stop) {
				state = Stop;
				auto n = endOfSession(buffer);
				if (n) inputs[0]->push(nullptr);
				return n;
			} else {
				return 0;
			}
		}

		curTransferedMeta = safe_cast<const MetadataFile>(curTransferedData->getMetadata());
		if (!open(curTransferedMeta)) {
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

	auto const read = gf_bs_read_data(curTransferedBs, (char*)buffer.ptr, std::min<u32>((u32)gf_bs_available(curTransferedBs), (u32)buffer.len));
	if (read == 0) {
		clean();
		return fillBuffer(buffer);
	} else {
		return read;
	}
}

bool HTTP::performTransfer() {
	CURLcode res = curl_easy_perform(curl);
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

