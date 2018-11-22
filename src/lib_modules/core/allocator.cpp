#include "allocator.hpp"
#include "lib_utils/log.hpp"
#include "lib_utils/format.hpp"

#include <stdexcept>

namespace Modules {

PacketAllocator::PacketAllocator(size_t /*unused*/, size_t maxBlocks) :
	maxBlocks(maxBlocks),
	curNumBlocks(maxBlocks) {
	if (maxBlocks == 0)
		throw std::runtime_error("Cannot create an allocator with 0 block.");
	allocatedBlockCount = 0;
	for (size_t i=0; i<maxBlocks; ++i) {
		eventQueue.push(Event{OneBufferIsFree});
	}
}

PacketAllocator::~PacketAllocator() {
	assert(allocatedBlockCount == 0);
}

void PacketAllocator::recycle(IBuffer *p) {
	delete p;
	allocatedBlockCount --;
	eventQueue.push(Event{OneBufferIsFree});
}
}
