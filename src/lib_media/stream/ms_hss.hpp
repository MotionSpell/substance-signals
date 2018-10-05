#pragma once

#include "lib_modules/core/module.hpp"
#include "../out/http.hpp"

namespace Modules {
namespace Stream {

class MS_HSS : public ModuleS, private Out::HTTP::Controller {
	public:
		MS_HSS(IModuleHost* host, const std::string &url);

		void process(Data data) override;
		void flush() override;

	private:
		void newFileCallback(span<uint8_t> out) final; //remove ftyp/moov
		void skipBox(uint32_t name, span<uint8_t> out);

		std::unique_ptr<Out::HTTP> m_http;
		IModuleHost* const m_host;
};

}
}
