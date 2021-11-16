#pragma once

#include <string>
#include "lib_media/common/utc_start_time.hpp"

struct TtmlDecoderConfig {
	IUtcStartTimeQuery const * utcStartTime = &g_NullStartTime;
};

