#pragma once

#include "lib_modules/core/buffer.hpp" // span
#include <functional>
#include <map>
#include <string>

typedef void NodeStartFunc(std::string, std::map<std::string, std::string>&);
typedef void NodeEndFunc(std::string);

void saxParse(span<const char> input, std::function<NodeStartFunc> onNodeStart, std::function<NodeEndFunc> onNodeEnd);

