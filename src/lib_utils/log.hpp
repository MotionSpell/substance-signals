#pragma once

#include "format.hpp"

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
		static void msg(Level level, const char* fmt, Arguments... args) {
			if ((level != Quiet) && (level <= globalLogLevel)) {
				auto msg = format(fmt, args...);
				send(level, msg.c_str());
			}
		}

		static void setLevel(Level level);
		static Level getLevel();

		static void setColor(bool isColored);
		static bool getColor();

		static void setSysLog(bool isSysLog);

	private:
		Log() {}
		static void send(Level level, const char* msg);

		static Level globalLogLevel;
		static bool globalColor;
		static bool globalSysLog;
		static void sendToSyslog(int level, const char* msg);
};

Level parseLogLevel(const char* level);

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

