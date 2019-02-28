#pragma once

#include "lib_modules/core/module.hpp"
#include "../out/http.hpp"

namespace Modules {
namespace Stream {

class MS_HSS : public ModuleS {
	public:
		MS_HSS(KHost* host, const std::string &url);
		virtual ~MS_HSS();

		void processOne(Data data) override;
		void flush() override;

	private:
		std::unique_ptr<HttpSender> m_httpSender;
		KHost* const m_host;
};

}
}
