#pragma once

#include <string>
#include <vector>

struct HttpOutputConfig {
	struct Flags {
		bool InitialEmptyPost = true;
		bool UsePUT = false; //use PUT instead of POST
	};

	std::string url;
	std::string userAgent = "GPAC Signals/";
	std::vector<std::string> headers {};

	// optional: data to transfer just before closing the connection
	std::vector<uint8_t> endOfSessionSuffix {};

	Flags flags {};
};

#include "lib_modules/utils/helper.hpp"

namespace Modules {
namespace Out {

struct Private;

class HTTP : public ModuleS {
	public:

		HTTP(IModuleHost* host, HttpOutputConfig const& cfg);
		virtual ~HTTP();

		void process(Data data) final;
		void flush() final;

		struct Controller {
			virtual void newFileCallback(span<uint8_t>) {}
		};
		Controller* m_controller = &m_nullController;

		void readTransferedBs(uint8_t* dst, size_t size);

	private:
		bool loadNextData();
		void clean();
		size_t fillBuffer(span<uint8_t> buffer);
		static size_t staticCurlCallback(void *ptr, size_t size, size_t nmemb, void *userp);

		IModuleHost* const m_host;

		std::unique_ptr<Private> m_pImpl;
		Data m_currData;
		span<const uint8_t> m_currBs {}; // points into the contents of m_currData

		const std::vector<uint8_t> endOfSessionSuffix;
		OutputDataDefault<DataRaw>* outputFinished;

		Controller m_nullController;
};

}
}
