#pragma once

#include "format.hpp"
#include <ostream>
#ifndef _WIN32
#include <syslog.h>
#endif

#define LOG_MSG_REPETITION_MAX 100

enum Level {
	Quiet = -1,
	Error = 0,
	Warning,
	Info,
	Debug
};

const int levelToSysLog[] = {3, 4, 6, 7};

class Log {
	public:
		template<typename... Arguments>
		static void msg(Level level, const std::string& fmt, Arguments... args) {
			if ((level != Quiet) && (level <= globalLogLevel)) {
#ifndef _WIN32
				if (globalSysLog) {
					::syslog(levelToSysLog[level], format(fmt, args...).c_str());
				} else
#endif
				{
					get(level) << getColorBegin(level) << getTime() << format(fmt, args...) << getColorEnd(level) << std::endl;
					get(level).flush();
				}
			}
		}

		static void setLevel(Level level);
		static Level getLevel();

		static void setColor(bool isColored);
		static bool getColor();

#ifndef _WIN32
		static void setSysLog(bool isSysLog);
		static bool getSysLog();
#endif

	private:
		Log() {}
		static std::ostream& get(Level level);
		static std::string getTime();
		static std::string getColorBegin(Level level);
		static std::string getColorEnd(Level level);

		static Level globalLogLevel;
		static bool globalColor;
#ifndef _WIN32
		static bool globalSysLog;
#endif
};

class LogRepetition {
public:
	LogRepetition() {}

	template<typename... Arguments>
	void msg(Level l, const std::string& fmt, Arguments... args) {
		if ((level != Quiet) && (l <= level)) {
			auto const msg = format(fmt, args...);
			if (lastMsgCount < LOG_MSG_REPETITION_MAX && l == lastLevel && msg == lastMsg) {
				lastMsgCount++;
			} else {
				if (lastMsgCount) {
					Log::msg(l, "Last message repeated %s times.", lastMsgCount);
				}

				Log::msg(l, msg);
				lastLevel = l;
				lastMsg = msg;
				lastMsgCount = 0;
			}
		}
	}

private:
	Level level = Log::getLevel(), lastLevel = Debug;
	std::string lastMsg;
	uint64_t lastMsgCount = 0;
};
