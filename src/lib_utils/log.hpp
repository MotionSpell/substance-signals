#pragma once

#include "format.hpp"
#include <ostream>

#define LOG_MSG_REPETITION_MAX 100
//#define LOG_THREAD_SAFETY //FIXME: crash seens

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
#ifdef LOG_THREAD_SAFETY
				if (lastMsgCount < LOG_MSG_REPETITION_MAX && fmt == lastMsg) {
					lastMsgCount++;
				} else {
					if (lastMsgCount) {
						get(level) << getColorBegin(level) << getTime() << format("Last message repeated %s times.", lastMsgCount) << getColorEnd(level) << std::endl;
					}
#endif	
					get(level) << getColorBegin(level) << getTime() << format(fmt, args...) << getColorEnd(level) << std::endl;
					get(level).flush();
#ifdef LOG_THREAD_SAFETY
					lastMsg = fmt;
					lastMsgCount = 0;
				}
#endif
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
#ifdef LOG_THREAD_SAFETY
		static std::string lastMsg;
		static uint64_t lastMsgCount;
#endif
};
