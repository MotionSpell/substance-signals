#pragma once

#include "lib_modules/core/module.hpp"

typedef void CURL;

namespace Modules {
namespace Stream {

class MS_HSS : public ModuleDynI {
	public:
		MS_HSS(const std::string &url, uint64_t segDurationInMs);
		virtual ~MS_HSS();

		void process() override final;
		void flush() override final;

	private:
		enum State {
			Init,
			RunNewConnection, //untouched, send from previous ftyp/moov
			RunNewFile,       //remove ftyp/moov
			RunResume,        //untouched
			Stop,
		};

		void endOfStream();

		void threadProc();
		int numDataQueueNotify = 0;
		std::thread workingThread;
		FILE *curTransferedFile = nullptr;
		Data curTransferedData;
		uint64_t curTransferedDataInputIndex = 0;
		static size_t staticCurlCallback(void *ptr, size_t size, size_t nmemb, void *userp);
		size_t curlCallback(void *ptr, size_t size, size_t nmemb);

		std::string url;
		CURL *curl;
		State state = Init;
};

}
}
