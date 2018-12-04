#pragma once

#include "log_sink.hpp"
extern LogSink* g_Log;

void setGlobalSyslog(bool enable);

void setGlobalLogLevel(Level level);
void setGlobalLogColor(bool enable);

