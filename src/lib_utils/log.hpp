#pragma once

#include "format.hpp"
#include <ostream>

enum Level {
	Quiet = -1,
	Error = 0,
	Warning,
	Info,
	Debug
};

class Log {
	public:
		template<typename... Arguments>
		static void msg(Level level, const std::string& fmt, Arguments... args) {
			if ((level != Quiet) && (level <= globalLogLevel)) {
				get(level) << getColorBegin(level) << getTime() << format(fmt, args...) << getColorEnd(level) << std::endl;
				get(level).flush();
			}
		}

		static void setLevel(Level level);
		static Level getLevel();

	private:
		Log() {}
		static std::ostream& get(Level level);
		static std::string getTime();
		static std::string getColorBegin(Level level);
		static std::string getColorEnd(Level level);

		static Level globalLogLevel;
};
