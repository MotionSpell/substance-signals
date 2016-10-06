#include "ms_hss.hpp"
#include "../common/libav.hpp"

extern "C" {
#include <curl/curl.h>
#include <gpac/tools.h>
}

#define CURL_DEBUG
//#define CURL_CHUNKED

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

#ifdef CURL_CHUNKED
	//curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	//curl_easy_setopt(curl, CURLOPT_POST, 1L);
#ifdef CURL_DEBUG
	//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif

	//make an empty POST to check the end point exists :
	//curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0);
	//CURLcode res = curl_easy_perform(curl);
	//if (res != CURLE_OK) {
	//	Log::msg(Warning, "curl_easy_perform() failed: %s", curl_easy_strerror(res));
	//	throw error("curl_easy_perform() failed");
	//}
	//curl_easy_reset(curl);

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
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
	{
		struct curl_slist *chunk = nullptr;
		chunk = curl_slist_append(chunk, "Expect:");
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
		state = Run;
		numDataQueueNotify = (int)getNumInputs() - 1; //FIXME: connection/disconnection cannot occur dynamically. Lock inputs?
		workingThread = std::thread(&MS_HSS::threadProc, this);
	}
}

size_t MS_HSS::staticCurlCallback(void *ptr, size_t size, size_t nmemb, void *userp) {
	auto pThis = (MS_HSS*)userp;
	return pThis->curlCallback(ptr, size, nmemb);
}

size_t MS_HSS::curlCallback(void *ptr, size_t size, size_t nmemb) {
	if (!curTransferedData) {
		curTransferedData = inputs[curTransferedDataInputIndex]->pop();
		if (!curTransferedData) {
			state = Stop;
			return 0;
		}

		std::shared_ptr<const MetadataFile> meta = safe_cast<const MetadataFile>(curTransferedData->getMetadata());
		if (!meta)
			throw error(format("Unknown data received on input %s", curTransferedDataInputIndex));
		curTransferedFile = fopen(meta->getFilename().c_str(), "rb");
	}

	auto transferSize = size*nmemb;
	auto const read = fread(ptr, 1, transferSize, curTransferedFile);

	if (read == 0) {//Romain: let's close the transfer for now:     < transferSize) {
		if (curTransferedFile) {
			fclose(curTransferedFile);
			curTransferedFile = nullptr;
			//gf_delete_file(safe_cast<const MetadataFile>(curTransferedData->getMetadata())->getFilename().c_str()); //Romain: not all are deleted...
			curTransferedData = nullptr;
			curTransferedDataInputIndex = (curTransferedDataInputIndex + 1) % inputs.size();
		}
#if 1 //Romain
	}
	return read;
#else
		return curlCallback((void*)((u8*)ptr + read), transferSize - read, 1) + read;
	} else {
		return read;
	}
#endif
}

void MS_HSS::threadProc() {
#ifndef CURL_CHUNKED
	const int transferSize = 1000000;
	void *ptr = (void*)malloc(transferSize); //Romain: to be freed
	while (state == Run) {
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
		auto const read = fread(ptr, 1, transferSize, curTransferedFile);
		fclose(curTransferedFile);
		curTransferedFile = nullptr;
		//gf_delete_file(safe_cast<const MetadataFile>(curTransferedData->getMetadata())->getFilename().c_str()); //Romain: not all are deleted...
		curTransferedData = nullptr;
		curTransferedDataInputIndex = (curTransferedDataInputIndex + 1) % inputs.size();

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, ptr);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)read);
#else
	while (state == Run) {
		//TODO: with the additional requirement that the encoder MUST resend the previous two MP4 fragments for each track in the stream, and resume without introducing discontinuities in the media timeline. Resending the last two MP4 fragments for each track ensures that there is no data loss.
#endif
		CURLcode res = curl_easy_perform(curl);
		if (res != CURLE_OK) {
			throw error(format("curl_easy_perform() failed: %s", curl_easy_strerror(res)));
		}

#if 0 //Romain
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		exit(1); //Romain: first segment only
#endif
	}
}

}
}
