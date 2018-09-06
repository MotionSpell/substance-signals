#pragma once

#include <stdint.h>

struct TsMuxConfig {
	bool real_time = false;
	unsigned mux_rate = 1000 * 1000;
	unsigned pcr_ms = 100;
	int64_t pcr_init_val = -1;
};

