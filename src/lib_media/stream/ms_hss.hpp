#pragma once

#include "lib_modules/core/module.hpp"
#include "../out/http.hpp"

namespace Modules {
namespace Stream {

class MS_HSS : public Out::HTTP {
	public:
		MS_HSS(const std::string &url);

	private:
		void newFileCallback(void *ptr) final;             //remove ftyp/moov
		size_t endOfSession(void *ptr, size_t size) final; //empty mfra
};

}
}
