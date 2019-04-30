#pragma once

#include <vector>
#include "buffer.hpp"

namespace Modules {

struct RawBuffer : IBuffer {
	RawBuffer(size_t size) : memoryBlock(size) {}
	std::vector<uint8_t> memoryBlock;

	Span data() {
		return Span { memoryBlock.data(), memoryBlock.size() };
	}

	SpanC data() const {
		return SpanC { memoryBlock.data(), memoryBlock.size() };
	}

	void resize(size_t size) {
		memoryBlock.resize(size);
	}
};

}

