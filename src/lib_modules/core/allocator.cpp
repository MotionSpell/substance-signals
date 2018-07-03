#include "allocator.hpp"
#include "lib_utils/log.hpp"

#include <stdexcept>

namespace Modules {

PacketAllocator::PacketAllocator(size_t minBlocks, size_t maxBlocks) :
	minBlocks(minBlocks),
	maxBlocks(maxBlocks),
	curNumBlocks(minBlocks) {
	if (minBlocks == 0)
		throw std::runtime_error("Cannot create an allocator with 0 block.");
	if (maxBlocks < minBlocks) {
		Log::msg(Warning, "Max block number %s is smaller than min block number %s. Aligning values.", maxBlocks, minBlocks);
		maxBlocks = minBlocks;
	}
	for (size_t i=0; i<minBlocks; ++i) {
		freeBlocks.push(Block{});
	}
}

PacketAllocator::~PacketAllocator() {
	Block block;
	while (freeBlocks.tryPop(block)) {
		delete block.data;
	}
}

}
