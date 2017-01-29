#pragma once

#include "lib_modules/core/module.hpp"

typedef void CURL;
struct curl_slist;
typedef struct __tag_bitstream GF_BitStream;

namespace Modules {
namespace Out {

class HTTP : public ModuleDynI {
	public:
		enum Flag {
			InitialEmptyPost = 1,
			Chunked          = 1 << 1, //not enabling it is experimental
		};

		HTTP(const std::string &url, Flag flags = InitialEmptyPost | Chunked);
		virtual ~HTTP();

		void process() final;
		void flush() final;

	protected:
		virtual void newFileCallback(void *) {}
		GF_BitStream *curTransferedBs = nullptr;

	private:
		enum State {
			Init,
			RunNewConnection, //untouched, send from previous ftyp/moov
			RunNewFile,       //execute newFileCallback()
			RunResume,        //untouched
			Stop,
		};

		void open(std::shared_ptr<const MetadataFile> meta);
		void clean();
		void endOfStream();

		void threadProc();
		bool performTransfer();
		int numDataQueueNotify = 0;
		std::thread workingThread;
		FILE *curTransferedFile = nullptr;
		Data curTransferedData;
		size_t curTransferedDataInputIndex = 0;
		static size_t staticCurlCallback(void *ptr, size_t size, size_t nmemb, void *userp);
		size_t curlCallback(void *ptr, size_t size, size_t nmemb);

		const std::string url;
		CURL *curl;
		struct curl_slist *chunk = nullptr;
		State state = Init;
		Flag flags;
};

}
}
