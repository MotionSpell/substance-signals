#pragma once

#include "lib_modules/core/buffer.hpp" // span
#include <lib_utils/small_map.hpp>
#include <functional>
#include <string>

typedef void NodeStartFunc(std::string /*id*/, SmallMap<std::string, std::string>& /*attributes*/);
typedef void NodeEndFunc(std::string /*id*/, std::string /*content*/);

void saxParse(span<const char> input, std::function<NodeStartFunc> onNodeStart, std::function<NodeEndFunc> onNodeEnd);

