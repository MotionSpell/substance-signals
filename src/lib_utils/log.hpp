#pragma once

#include "lib_utils/format.hpp"
#include "log_sink.hpp"
extern LogSink* g_Log;

void setGlobalLogLevel(Level level);
void setGlobalSyslog(bool enable);

// TODO: get rid of this
Level parseLogLevel(const char* level);

