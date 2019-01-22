#include "allocator.hpp"
#include "lib_utils/log.hpp"
#include "lib_utils/format.hpp"

#include <stdexcept>
#include <cassert>

namespace Modules {

PacketAllocator::PacketAllocator(size_t maxBlocks) :
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

void* PacketAllocator::alloc(size_t size) {
	Event block;
	if (!eventQueue.tryPop(block)) {
		if (curNumBlocks < maxBlocks) {
			eventQueue.push(Event{OneBufferIsFree});
			curNumBlocks++;
		}
		block = eventQueue.pop();
	}
	switch (block.type) {
	case OneBufferIsFree: {
		allocatedBlockCount ++;
		return new uint8_t[size];
	}
	case Exit:
		return nullptr;
	}
	return nullptr;
}

void PacketAllocator::recycle(void* p) {
	delete [] (uint8_t*)p;
	allocatedBlockCount --;
	eventQueue.push(Event{OneBufferIsFree});
}
}
