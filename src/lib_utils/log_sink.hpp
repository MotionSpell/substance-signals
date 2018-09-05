#pragma once

enum Level {
	Quiet = -1,
	Error = 0,
	Warning,
	Info,
	Debug
};

struct LogSink {
	virtual void log(Level level, const char* msg) = 0;
};
