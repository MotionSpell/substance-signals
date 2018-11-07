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
		std::shared_ptr<T> getBuffer(size_t size, std::shared_ptr<PacketAllocator> allocator) {
			Event block;
			if (!eventQueue.tryPop(block)) {
				if (curNumBlocks < maxBlocks) {
					eventQueue.push(Event{});
					curNumBlocks++;
				}
				block = eventQueue.pop();
			}
			switch (block.type) {
			case OneBufferIsFree: {
				if (!block.data) {
					block.data = new T(size);
				}
				if (block.data->data().len < size) {
					block.data->resize(size);
				}
				auto ret = std::shared_ptr<T>(safe_cast<T>(block.data), Deleter{allocator});
				return ret;
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
			IBuffer*data = nullptr;
		};

		const size_t minBlocks;
		const size_t maxBlocks;
		std::atomic_size_t curNumBlocks;
		Queue<Event> eventQueue;
};

}
