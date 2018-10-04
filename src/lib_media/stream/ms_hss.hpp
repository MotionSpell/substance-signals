#pragma once

#include "lib_modules/core/module.hpp"
#include "../out/http.hpp"

namespace Modules {
namespace Stream {

class MS_HSS : public Out::HTTP {
	public:
		MS_HSS(IModuleHost* host, const std::string &url);

	private:
		void newFileCallback(span<uint8_t> out) final; //remove ftyp/moov
		bool endOfSession(span<uint8_t> buffer) final; //empty mfra

		IModuleHost* const m_host;
};

}
}
