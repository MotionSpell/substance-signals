#pragma once

#include "lib_modules/core/database.hpp" // Data
#include "lib_modules/core/buffer.hpp" // SpanC
#include <vector>

struct PesPacket {
	int64_t dts;
	std::vector<uint8_t> data;
};

PesPacket createPesPacket(int streamId, Modules::Data data);
