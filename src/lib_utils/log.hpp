#pragma once

#include "format.hpp"
#include <string>

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
				auto msg = format(fmt, args...);
				send(level, msg);
			}
		}

		static void setLevel(Level level);
		static Level getLevel();

		static void setColor(bool isColored);
		static bool getColor();

		static void setSysLog(bool isSysLog);
		static bool getSysLog();

	private:
		Log() {}
		static void send(Level level, std::string const& msg);
		static std::string getTime();
		static std::string getColorBegin(Level level);
		static std::string getColorEnd(Level level);

		static Level globalLogLevel;
		static bool globalColor;
		static bool globalSysLog;
		static void sendToSyslog(int level, std::string msg);
};

Level parseLogLevel(std::string level);

struct ScopedLogLevel {
		ScopedLogLevel(Level level) : oldLevel(Log::getLevel()) {
			Log::setLevel(level);
		}
		~ScopedLogLevel() {
			Log::setLevel(oldLevel);
		}
	private:
		const Level oldLevel;
};

