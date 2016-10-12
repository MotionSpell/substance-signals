#include "ms_hss.hpp"
#include "../common/libav.hpp"

extern "C" {
#include <curl/curl.h>
#include <gpac/tools.h>
}

#define CURL_DEBUG
#define CURL_CHUNKED
#define INIT_POST
#define SKIP_MOOV
#define U32LE(p) ((((p)[0]) << 24) | (((p)[1]) << 16) | (((p)[2]) << 8) | ((p)[3]))

namespace Modules {

namespace Stream {

MS_HSS::MS_HSS(const std::string &url, uint64_t segDurationInMs)
: url(url) {
	if (url.compare(0, 7, "http://"))
		throw error(format("can only handle URLs startint with 'http://', not %s.", url));

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (!curl)
		throw error("couldn't init the HTTP stack.");

#ifdef INIT_POST
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
#ifdef CURL_DEBUG
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif

	//make an empty POST to check the end point exists :
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0);
	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		log(Warning, "curl_easy_perform() failed: %s", curl_easy_strerror(res));
		throw error("curl_easy_perform() failed");
	}
	curl_easy_reset(curl);
#endif

#ifdef CURL_CHUNKED
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

#ifdef CURL_DEBUG
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, &MS_HSS::staticCurlCallback);
	curl_easy_setopt(curl, CURLOPT_READDATA, this);
	{
		struct curl_slist *chunk = nullptr;
		chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	}
#endif

	addInput(new Input<DataAVPacket>(this));
}

MS_HSS::~MS_HSS() {
	endOfStream();
	//TODO: curl_slist_free_all();
	curl_easy_cleanup(curl);
	curl_global_cleanup();
}

void MS_HSS::endOfStream() {
	if (workingThread.joinable()) {
		for (size_t i = 0; i < inputs.size(); ++i) {
			inputs[i]->push(nullptr);
		}
		workingThread.join();
	}
}

void MS_HSS::flush() {
	numDataQueueNotify--;
	if (numDataQueueNotify == 0) {
		endOfStream();
	}
}

void MS_HSS::process() {
	if (!workingThread.joinable() && state == Init) {
		state = RunNewConnection;
		numDataQueueNotify = (int)getNumInputs() - 1; //FIXME: connection/disconnection cannot occur dynamically. Lock inputs?
		workingThread = std::thread(&MS_HSS::threadProc, this);
	}
}

size_t MS_HSS::staticCurlCallback(void *ptr, size_t size, size_t nmemb, void *userp) {
	auto pThis = (MS_HSS*)userp;
	return pThis->curlCallback(ptr, size, nmemb);
}

size_t MS_HSS::curlCallback(void *ptr, size_t size, size_t nmemb) {
	if (state == RunNewConnection && curTransferedData) {
		std::shared_ptr<const MetadataFile> meta = safe_cast<const MetadataFile>(curTransferedData->getMetadata());
		log(Debug, "reconnect: file %s", meta->getFilename());
		gf_fseek(curTransferedFile, 0, SEEK_SET);
	}

	if (!curTransferedData) {
		curTransferedData = inputs[curTransferedDataInputIndex]->pop();
		if (!curTransferedData) {
			state = Stop;
			return 0;
		}

		std::shared_ptr<const MetadataFile> meta = safe_cast<const MetadataFile>(curTransferedData->getMetadata());
		if (!meta)
			throw error(format("Unknown data received on input %s", curTransferedDataInputIndex));
		curTransferedFile = gf_fopen(meta->getFilename().c_str(), "rb");
		if (state != RunNewConnection) {
			state = RunNewFile; //on new connection, don't remove the ftyp/moov
		}
	}

	if (state == RunNewConnection) {
		state = RunResume;
	}
#ifdef SKIP_MOOV
	else if (state == RunNewFile) {
		auto const data = (u8*)ptr;
		auto read = fread(ptr, 1, 8, curTransferedFile);
		if (read != 8)
			throw error("I/O error (1)");
		u32 size = U32LE(data);
		u32 type = U32LE(data + 4);
		if (type != GF_4CC('f', 't', 'y', 'p'))
			throw error("ftyp not found");
		read = fread(ptr, 1, size - 8, curTransferedFile);
		if (read != size - 8)
			throw error("I/O error (2)");

		read = fread(ptr, 1, 8, curTransferedFile);
		if (read != 8)
			throw error("I/O error (3)");
		size = U32LE(data);
		read = fread(ptr, 1, size - 8, curTransferedFile);
		if (read != size - 8)
			throw error("I/O error (4)");

		read = fread(ptr, 1, 8, curTransferedFile);
		if (read != 8)
			throw error("I/O error (5)");
		size = U32LE(data);
		type = U32LE(data + 4);
		if (type != GF_4CC('f', 'r', 'e', 'e'))
			throw error("moov not found");
		read = fread(ptr, 1, size - 8, curTransferedFile);
		if (read != size - 8)
			throw error("I/O error (6)");

		read = fread(ptr, 1, 8, curTransferedFile);
		if (read != 8)
			throw error("I/O error (7)");
		size = U32LE(data);
		type = U32LE(data + 4);
		if (type != GF_4CC('m', 'o', 'o', 'v'))
			throw error("moov not found");
		read = fread(ptr, 1, size - 8, curTransferedFile);
		if (read != size - 8)
			throw error("I/O error (8)");

		state = RunResume;
	}
#endif

	auto const transferSize = size*nmemb;
	auto const read = fread(ptr, 1, transferSize, curTransferedFile);
	if (read == 0) {
		if (curTransferedFile) {
			gf_fclose(curTransferedFile);
			curTransferedFile = nullptr;
			gf_delete_file(safe_cast<const MetadataFile>(curTransferedData->getMetadata())->getFilename().c_str());
			curTransferedData = nullptr;
			curTransferedDataInputIndex = (curTransferedDataInputIndex + 1) % inputs.size();
		}
		return curlCallback(ptr, transferSize, 1);
	} else {
		return read;
	}
}

void MS_HSS::threadProc() {
#ifndef CURL_CHUNKED //TODO: this is a test mode only ATM
	const int transferSize = 1000000;
	void *ptr = (void*)malloc(transferSize); //FIXME: to be freed
	while (state != Stop) {
		//TODO: with the additional requirement that the encoder MUST resend the previous two MP4 fragments for each track in the stream, and resume without introducing discontinuities in the media timeline. Resending the last two MP4 fragments for each track ensures that there is no data loss.
		auto curTransferedData = inputs[curTransferedDataInputIndex]->pop();
		if (!curTransferedData) {
			state = Stop;
			break;
		}
		std::shared_ptr<const MetadataFile> meta = safe_cast<const MetadataFile>(curTransferedData->getMetadata());
		if (!meta)
			throw error(format("Unknown data received on input %s", curTransferedDataInputIndex));
		curTransferedFile = fopen(meta->getFilename().c_str(), "rb");
		size_t read = fread(ptr, 1, transferSize, curTransferedFile), fileSize = read;
		while (read) {
			read = fread(ptr, 1, transferSize, curTransferedFile);
			fileSize += read;
		}
		fclose(curTransferedFile);
		curTransferedFile = nullptr;
		gf_delete_file(safe_cast<const MetadataFile>(curTransferedData->getMetadata())->getFilename().c_str());
		curTransferedData = nullptr;
		curTransferedDataInputIndex = (curTransferedDataInputIndex + 1) % inputs.size();

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, ptr);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)fileSize);
#else
	while (state != Stop) {
		//TODO: with the additional requirement that the encoder MUST resend the previous two MP4 fragments for each track in the stream, and resume without introducing discontinuities in the media timeline. Resending the last two MP4 fragments for each track ensures that there is no data loss.
#endif
		CURLcode res = curl_easy_perform(curl);
		if (res == CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}

		if (state == Stop) {
			break;
		} else {
			state = RunNewConnection;
		}
	}
}

}
}
