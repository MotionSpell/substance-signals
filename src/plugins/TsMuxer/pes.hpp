#pragma once

#include "lib_modules/core/database.hpp" // Data
#include "lib_modules/core/buffer.hpp" // SpanC
#include <vector>

std::vector<uint8_t> createPesPacket(int streamId, Modules::Data data);
