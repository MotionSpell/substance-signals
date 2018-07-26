#pragma once

#include <cstdint>

namespace Pipelines {

struct StatsEntry {
	char name[252] {};
	int32_t value = 0;
};

static_assert(sizeof(StatsEntry) == 256);

struct IStatsRegistry {
	virtual ~IStatsRegistry() {}
	virtual StatsEntry* getNewEntry() = 0; /*owned by the StatsRegistry object*/
};

}
