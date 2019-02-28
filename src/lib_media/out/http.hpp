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

#include "lib_modules/core/module.hpp"
#include "lib_modules/utils/helper.hpp"

struct HttpSender;

namespace Modules {
namespace Out {

class HTTP : public ModuleS {
	public:

		HTTP(KHost* host, HttpOutputConfig const& cfg);
		virtual ~HTTP();

		void processOne(Data data) final;
		void flush() final;

	private:
		KHost* const m_host;
		std::unique_ptr<HttpSender> m_sender;
		std::vector<uint8_t> m_suffixData;
		OutputDefault* outputFinished;
};

}
}
