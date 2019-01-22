#pragma once

#include "lib_utils/queue.hpp"
#include <atomic>

namespace Modules {

/*user recommended values*/
static const size_t ALLOC_NUM_BLOCKS_DEFAULT = 10;
static const size_t ALLOC_NUM_BLOCKS_LOW_LATENCY = 2;

class PacketAllocator {
	public:
		PacketAllocator(size_t maxBlocks);
		~PacketAllocator();

		void* alloc(size_t size);
		void recycle(void* p);

		void unblock() {
			eventQueue.push(Event{Exit});
		}

	private:

		enum EventType {
			OneBufferIsFree,
			Exit,
		};
		struct Event {
			EventType type {};
		};

		const size_t maxBlocks;
		std::atomic_size_t curNumBlocks;
		Queue<Event> eventQueue;

		// Count of blocks 'in the wild'.
		// Only used for sanity-checking at destruction time.
		std::atomic<int> allocatedBlockCount;
};
}

#include <memory>

namespace Modules {
template<typename T>
std::shared_ptr<T> alloc(std::shared_ptr<PacketAllocator> allocator, size_t size) {
	auto p = allocator->alloc(sizeof(T));

	auto deleter = [allocator](T* p) {
		p->~T();
		allocator->recycle(p);
	};

	return std::shared_ptr<T>(new(p) T(size), deleter);
}

}
