#pragma once

#include "lib_utils/log.hpp"

namespace Modules {

struct LogCap : public LogRepetition {
	virtual ~LogCap() noexcept(false) {}

	template<typename... Arguments>
	void log(Level level, const std::string& fmt, Arguments... args) {
		msg(level, "[%s %s] " + fmt, this, typeid(*this).name(), args...);
	}
};

}
