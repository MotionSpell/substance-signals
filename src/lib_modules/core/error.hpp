#pragma once

#include <stdexcept>
#include <string>
#include "lib_utils/format.hpp"

namespace Modules {

class ErrorCap {
	protected:
		std::exception error(std::string const &msg) const {
			throw std::runtime_error(format("[%s] %s", typeid(*this).name(), msg));
		}
};

}
