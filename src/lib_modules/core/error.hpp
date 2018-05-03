#pragma once

#include "lib_utils/log.hpp"
#include <stdexcept>
#include <string>

namespace Modules {

class Exception : public std::runtime_error {
	public:
		Exception(std::string const &msg) throw() : std::runtime_error(msg) {
			Log::msg(Error, "Caught exception: %s", msg);
		}
		virtual ~Exception() throw() {}

	private:
		Exception& operator= (const Exception&) = delete;
};

class ErrorCap {
	protected:
		std::exception error(std::string const &msg) const {
			throw Exception(format("[%s] %s", typeid(*this).name(), msg));
		}
};

}
