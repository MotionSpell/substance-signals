#pragma once

#include <stdexcept>
#include <string>

namespace Modules {

class ErrorCap {
	protected:
		std::exception error(std::string const &msg) const {
			return std::runtime_error("[" + std::string(typeid(*this).name()) + "]" + msg);
		}
};

}
