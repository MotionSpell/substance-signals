#pragma once

#include "buffer.hpp"
#include "lib_utils/queue.hpp"
#include "lib_utils/tools.hpp"
#include <atomic>
#include <memory>

namespace Modules {

/*user recommended values*/
static const size_t ALLOC_NUM_BLOCKS_DEFAULT = 10;
static const size_t ALLOC_NUM_BLOCKS_LOW_LATENCY = 2;

class PacketAllocator {
	public:
		PacketAllocator(size_t minBlocks, size_t maxBlocks);
		~PacketAllocator();

		struct Deleter {
			void operator()(IBuffer *p) const {
				allocator->recycle(p);
			}
			std::shared_ptr<PacketAllocator> const allocator;
		};

		template<typename T>
		std::shared_ptr<T> alloc(size_t size, std::shared_ptr<PacketAllocator> allocator) {
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
				auto data = new T(size);
				return std::shared_ptr<T>(data, Deleter{allocator});
			}
			case Exit:
				return nullptr;
			}
			return nullptr;
		}

		void unblock() {
			eventQueue.push(Event{Exit});
		}

	private:
		void recycle(IBuffer *p);

		enum EventType {
			OneBufferIsFree,
			Exit,
		};
		struct Event {
			EventType type {};
		};

		const size_t minBlocks;
		const size_t maxBlocks;
		std::atomic_size_t curNumBlocks;
		std::atomic<int> allocatedBlockCount; // number of blocks 'in the wild'
		Queue<Event> eventQueue;
};

}
