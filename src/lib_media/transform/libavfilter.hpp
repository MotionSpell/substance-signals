#pragma once

#include <string>
#include "../common/picture.hpp"

struct AvFilterConfig {
	Modules::PictureFormat format;
	std::string filterArgs;
};

