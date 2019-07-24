#pragma once

#include <string>
#include <vector>
#include "../common/http_sender.hpp"

struct HttpOutputConfig {
	struct Flags {
		bool InitialEmptyPost = true;
		HttpRequest request = POST;
	};

	std::string url;
	std::string userAgent = "GPAC Signals/";
	std::vector<std::string> headers {};

	// optional: data to transfer just before closing the connection
	std::vector<uint8_t> endOfSessionSuffix {};

	Flags flags {};
};

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
