#include "log.hpp"
#include "clock.hpp"
#include <cassert>
#include <ctime>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <wincon.h>
static HANDLE console = NULL;
static WORD console_attr_ori = 0;
#else /*_WIN32*/
#include <syslog.h>
const int levelToSysLog[] = { 3, 4, 6, 7 };
#define RED    "\x1b[31m"
#define YELLOW "\x1b[33m"
#define GREEN  "\x1b[32m"
#define CYAN   "\x1b[36m"
#define WHITE  "\x1b[37m"
#define RESET  "\x1b[0m"
bool Log::globalSysLog = false;
#endif /*_WIN32*/

Level Log::globalLogLevel = Warning;
bool Log::globalColor = true;

std::ostream& Log::get(Level level) {
	switch (level) {
	case Info:
		return std::cout;
	default:
		return std::cerr;
	}
}

std::string Log::getTime() {
	char szOut[255];
	const std::time_t t = std::time(nullptr);
	const std::tm tm = *std::gmtime(&t);
	auto const size = strftime(szOut, 255, "%Y/%m/%d %H:%M:%S", &tm);
	return format("[%s][%s] ", std::string(szOut, size), (double)g_DefaultClock->now());
}

std::string Log::getColorBegin(Level level) {
	if (!getColor()) return "";
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

std::string Log::getColorEnd(Level /*level*/) {
	if (!getColor()) return "";
#ifdef _WIN32
	SetConsoleTextAttribute(console, console_attr_ori);
#else
	fprintf(stderr, RESET);
#endif
	return "";
}

void Log::setLevel(std::string level) {
	if (level == "error") {
		globalLogLevel = Error;
	} else if (level == "warning") {
		globalLogLevel = Warning;
	} else if (level == "info") {
		globalLogLevel = Info;
	} else if (level == "debug") {
		globalLogLevel = Debug;
	} else
		throw std::runtime_error(format("Unknown log level: \"%s\"", level));
}

void Log::setLevel(Level level) {
	globalLogLevel = level;
}

Level Log::getLevel() {
	return globalLogLevel;
}

void Log::setColor(bool isColored) {
	globalColor = isColored;
}

bool Log::getColor() {
	return globalColor;
}

#ifndef _WIN32
void Log::setSysLog(bool isSysLog) {
	if (!globalSysLog && isSysLog) {
		openlog(nullptr, 0, LOG_USER);
	} else if (globalSysLog && !isSysLog) {
		closelog();
	}
	globalSysLog = isSysLog;
}

void Log::sendToSyslog(int level, std::string msg) {
	::syslog(levelToSysLog[level], "%s", msg.c_str());
}

bool Log::getSysLog() {
	return globalSysLog;
}
#endif
