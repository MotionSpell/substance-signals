#include "allocator.hpp"
#include "lib_utils/log.hpp"
#include "lib_utils/format.hpp"

#include <stdexcept>

auto const ALLOC_NUM_BLOCKS_MAX_DYN_FREE = 0 ;/*free the dynamically allocated blocks*/

namespace Modules {

PacketAllocator::PacketAllocator(size_t minBlocks, size_t maxBlocks) :
	minBlocks(minBlocks),
	maxBlocks(maxBlocks),
	curNumBlocks(minBlocks) {
	if (minBlocks == 0)
		throw std::runtime_error("Cannot create an allocator with 0 block.");
	if (maxBlocks < minBlocks) {
		g_Log->log(Warning, format("Max block number %s is smaller than min block number %s. Aligning values.", maxBlocks, minBlocks).c_str());
		maxBlocks = minBlocks;
	}
	for (size_t i=0; i<minBlocks; ++i) {
		eventQueue.push(Event{OneBufferIsFree});
	}
}

void PacketAllocator::recycle(IBuffer *p) {
	delete p;
	if(ALLOC_NUM_BLOCKS_MAX_DYN_FREE) {
		if (curNumBlocks > minBlocks) {
			curNumBlocks--;
			return;
		}
	}
	eventQueue.push(Event{OneBufferIsFree, nullptr});
}
}
