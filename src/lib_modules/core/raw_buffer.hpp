#pragma once

#include <vector>
#include "buffer.hpp"

namespace Modules {

struct RawBuffer : IBuffer {
		RawBuffer(size_t size) : memoryBlock(size), m_size(size) {}

		Span data() {
			return Span { memoryBlock.data(), m_size };
		}

		SpanC data() const {
			return SpanC { memoryBlock.data(), m_size };
		}

		// don't resize when shrinking
		void resize(size_t size) {
			m_size = size;
			if (size > memoryBlock.size())
				memoryBlock.resize(size);
		}

	private:
		std::vector<uint8_t> memoryBlock;
		size_t m_size = 0;
};

}

