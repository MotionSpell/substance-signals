#pragma once

#include "lib_utils/log.hpp"
#include <typeinfo>

namespace Modules {

struct LogCap : protected LogRepetition {
		virtual ~LogCap() {}

	protected:
		template<typename... Arguments>
		void log(Level level, const std::string& fmt, Arguments... args) {
			auto const id = format("[%s %s]", this, typeid(*this).name());
			msg(level, id, fmt, args...);
		}
};

}
