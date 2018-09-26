#pragma once

#include <stdint.h>

struct IUtcStartTimeQuery {
	virtual uint64_t query() = 0;
};

struct NullStartTime : IUtcStartTimeQuery {
	uint64_t query() override {
		return 0;
	}
};

static NullStartTime g_NullStartTime;

