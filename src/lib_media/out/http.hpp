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

struct HttpSender {
	virtual ~HttpSender() = default;
	virtual void send(Data data) = 0;
	virtual void setPrefix(Data data) = 0;
};

class HTTP : public ModuleS {
	public:

		HTTP(KHost* host, HttpOutputConfig const& cfg);
		virtual ~HTTP();

		void setPrefix(span<const uint8_t> prefix);

		void processOne(Data data) final;
		void flush() final;

	private:
		KHost* const m_host;
		std::unique_ptr<HttpSender> m_sender;
		Data m_suffixData;
		OutputWithAllocator* outputFinished;
};

}
}
