#pragma once

#include "lib_utils/log.hpp"
#include <stdexcept>
#include <string>

namespace Modules {

class ErrorCap {
	protected:
		std::exception error(std::string const &msg) const {
			throw std::runtime_error(format("[%s] %s", typeid(*this).name(), msg));
		}
};

}
