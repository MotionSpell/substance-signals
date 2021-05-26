#pragma once

#include "log_sink.hpp"
#include <string>

extern LogSink* g_Log;

void setGlobalLogSyslog(const char *ident, const std::string &channel_name);
void setGlobalLogConsole(bool color_enable);
void setGlobalLogCSV(const char* path);

Level getGlobalLogLevel();
void setGlobalLogLevel(Level level);

Level parseLogLevel(const char* slevel);
