#pragma once

#include "data.hpp"
#include "lib_utils/queue.hpp"
#include "lib_utils/tools.hpp"
#include <atomic>
#include <memory>

namespace Modules {

/*user recommended values*/
static const size_t ALLOC_NUM_BLOCKS_DEFAULT = 10;
static const size_t ALLOC_NUM_BLOCKS_LOW_LATENCY = 2;

//#define ALLOC_DEBUG_TRACK_BLOCKS

class PacketAllocator {
	public:
		PacketAllocator(size_t minBlocks, size_t maxBlocks);
		~PacketAllocator();

		struct Deleter {
			Deleter(std::shared_ptr<PacketAllocator> allocator) : allocator(allocator) {}
			void operator()(IData *p) const {
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
				} else {
					auto data = safe_cast<T>(block.data);
					if (data->size() < size) {
						data->resize(size);
					}
				}

				auto ret = std::shared_ptr<T>(safe_cast<T>(block.data), Deleter(allocator));
#ifdef ALLOC_DEBUG_TRACK_BLOCKS
				usedBlocks.push(ret);
#endif
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
		void recycle(IData *p);

		enum EventType {
			OneBufferIsFree,
			Exit,
		};
		struct Event {
			EventType type {};
			IData*data = nullptr;
		};

		const size_t minBlocks;
		const size_t maxBlocks;
		std::atomic_size_t curNumBlocks;
		Queue<Event> eventQueue;
#ifdef ALLOC_DEBUG_TRACK_BLOCKS
		Queue<std::weak_ptr<IData>> usedBlocks;
#endif
};

}
