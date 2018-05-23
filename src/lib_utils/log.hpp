#pragma once

#include "format.hpp"
#include <ostream>
#include <string>

#define LOG_MSG_REPETITION_MAX 100

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
#ifndef _WIN32
				if (globalSysLog) {
					sendToSyslog(level, msg);
				} else
#endif
				{
					get(level) << getColorBegin(level) << getTime() << msg << getColorEnd(level) << std::endl;
				}
			}
		}

		static void setLevel(std::string level);
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
		static void sendToSyslog(int level, std::string msg);
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
