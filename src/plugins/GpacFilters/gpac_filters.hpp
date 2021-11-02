#pragma once

#include <string>

namespace Modules {

/*
Signals wrapper for GPAC Filters.

The implementation is incomplete for now, see unit tests for coverage.
The goal is to port all the GPAC Signals modules to the Filters API.
*/

struct GpacFiltersConfig {
	std::string filterName;
};

}
