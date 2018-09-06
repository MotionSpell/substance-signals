#include "log.hpp"
#include "clock.hpp"
#include <cassert>
#include <ctime>
#include <iostream>
#include "lib_utils/system_clock.hpp"
#include "lib_utils/format.hpp"

#ifdef _WIN32
#include <windows.h>
#include <wincon.h>
static HANDLE console = NULL;
static WORD console_attr_ori = 0;
#else /*_WIN32*/
#include <syslog.h>
#define RED    "\x1b[31m"
#define YELLOW "\x1b[33m"
#define GREEN  "\x1b[32m"
#define CYAN   "\x1b[36m"
#define WHITE  "\x1b[37m"
#define RESET  "\x1b[0m"
#endif /*_WIN32*/

class Log : public LogSink {
	public:

		void log(Level level, const char* msg) override {
			if ((level != Quiet) && (level <= m_logLevel))
				send(level, msg);
		}

		void setLevel(Level level);
		void setColor(bool isColored);
		void setSysLog(bool isSysLog);

	private:
		void send(Level level, const char* msg);

		Level m_logLevel = Warning;
		bool m_color = true;
		bool m_syslog = false;
		void sendToSyslog(int level, const char* msg);

		std::string getColorBegin(Level level) {
			if (!m_color) return "";
#ifdef _WIN32
			if (console == NULL) {
				CONSOLE_SCREEN_BUFFER_INFO console_info;
				console = GetStdHandle(STD_ERROR_HANDLE);
				assert(console != INVALID_HANDLE_VALUE);
				if (console != INVALID_HANDLE_VALUE) {
					GetConsoleScreenBufferInfo(console, &console_info);
					console_attr_ori = console_info.wAttributes;
				}
			}
			switch (level) {
			case Error: SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_INTENSITY); break;
			case Warning: SetConsoleTextAttribute(console, FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN); break;
			case Info: SetConsoleTextAttribute(console, FOREGROUND_INTENSITY | FOREGROUND_GREEN); break;
			case Debug: SetConsoleTextAttribute(console, FOREGROUND_GREEN); break;
			default: break;
			}
#else
			switch (level) {
			case Error: fprintf(stderr, RED); break;
			case Warning: fprintf(stderr, YELLOW); break;
			case Info: fprintf(stderr, GREEN); break;
			case Debug: fprintf(stderr, CYAN); break;
			default: break;
			}
#endif
			return "";
		}

		std::string getColorEnd(Level /*level*/) {
			if (!m_color) return "";
#ifdef _WIN32
			SetConsoleTextAttribute(console, console_attr_ori);
#else
			fprintf(stderr, RESET);
#endif
			return "";
		}

};

static std::ostream& get(Level level) {
	switch (level) {
	case Info:
		return std::cout;
	default:
		return std::cerr;
	}
}

static std::string getTime() {
	char szOut[255];
	const std::time_t t = std::time(nullptr);
	const std::tm tm = *std::gmtime(&t);
	auto const size = strftime(szOut, 255, "%Y/%m/%d %H:%M:%S", &tm);
	return format("[%s][%s] ", std::string(szOut, size), (double)g_SystemClock->now());
}


void Log::send(Level level, const char* msg) {
	if (m_syslog) {
		sendToSyslog(level, msg);
	} else {
		get(level) << getColorBegin(level) << getTime() << msg << getColorEnd(level) << std::endl;
	}
}

void Log::setLevel(Level level) {
	m_logLevel = level;
}

void Log::setColor(bool isColored) {
	m_color = isColored;
}

void Log::setSysLog(bool isSysLog) {
#ifndef _WIN32
	if (!m_syslog && isSysLog) {
		openlog(nullptr, 0, LOG_USER);
	} else if (m_syslog && !isSysLog) {
		closelog();
	}
	m_syslog = isSysLog;
#else
	if(isSysLog)
		throw std::runtime_error("Syslog is not supported on this platform");
#endif
}

void Log::sendToSyslog(int level, const char* msg) {
#ifndef _WIN32
	static const int levelToSysLog[] = { 3, 4, 6, 7 };
	::syslog(levelToSysLog[level], "%s", msg);
#else
	(void)level;
	(void)msg;
#endif
}

static Log globalLogger;
LogSink* g_Log = &globalLogger;

void setGlobalLogLevel(Level level) {
	globalLogger.setLevel(level);
}

void setGlobalSyslog(bool enable) {
	globalLogger.setSysLog(enable);
}

void setGlobalLogColor(bool enable) {
	globalLogger.setColor(enable);
}

// TODO: get rid of this
Level parseLogLevel(const char* slevel) {
	auto level = std::string(slevel);
	if (level == "error") {
		return Error;
	} else if (level == "warning") {
		return Warning;
	} else if (level == "info") {
		return Info;
	} else if (level == "debug") {
		return Debug;
	} else
		throw std::runtime_error(format("Unknown log level: \"%s\"", level));
}

