#pragma once

#include <cstdint>

namespace Pipelines {

struct StatsEntry {
	char name[255] = { 0 };
	int32_t value = 0;
};
struct IStatsRegistry {
	virtual ~IStatsRegistry() {}
	virtual StatsEntry* getNewEntry() = 0; /*owned by the StatsRegistry object*/
};

}
