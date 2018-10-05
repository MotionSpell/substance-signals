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

		void setPrefix(span<const uint8_t> prefix);

		void process(Data data) final;
		void flush() final;

	private:
		IModuleHost* const m_host;
		std::unique_ptr<Private> m_pImpl;
		Data m_suffixData;
		OutputDataDefault<DataRaw>* outputFinished;
};

}
}
