#pragma once

#include <string>
#include <vector>

struct HttpOutputConfig {
	struct Flags {
		bool InitialEmptyPost = true;
		bool Chunked = true; //use HTTP chunked transfer mode (disabling it is experimental).
		bool UsePUT = false; //use PUT instead of POST
	};

	std::string url;
	std::string userAgent = "GPAC Signals/";
	std::vector<std::string> headers {};

	Flags flags {};
};

#include "../common/metadata.hpp"
#include "lib_modules/utils/helper.hpp"

namespace Modules {
namespace Out {

class HTTP : public Module {
	public:

		HTTP(IModuleHost* host, HttpOutputConfig const& cfg);
		virtual ~HTTP();

		void process() final;
		void flush() final;

		virtual void newFileCallback(span<uint8_t>) {}
		virtual bool endOfSession(span<uint8_t>) {
			return false;
		}
		void readTransferedBs(uint8_t* dst, size_t size);

	private:
		enum State {
			Init,
			RunNewConnection, //untouched, send from previous ftyp/moov
			RunNewFile,       //execute newFileCallback()
			RunResume,        //untouched
			Stop,             //execute endOfSession()
		};

		bool open();
		void clean();
		void endOfStream();

		void threadProc();
		bool performTransfer();

		IModuleHost* const m_host;

		struct Private;

		std::unique_ptr<Private> m_pImpl;

		Data m_currData;
		Metadata m_currMetadata;
		span<const uint8_t> m_currBs {}; // points into the contents of m_currData

		static size_t staticCurlCallback(void *ptr, size_t size, size_t nmemb, void *userp);
		size_t fillBuffer(span<uint8_t> buffer);

		const std::string url, userAgent;
		State state = Init;
		HttpOutputConfig::Flags flags;
		OutputDataDefault<DataRaw> *outputFinished;
};

}
}
