#pragma once

#include "data.hpp"
#include "lib_utils/log.hpp"
#include "lib_utils/queue.hpp"
#include "lib_utils/tools.hpp"
#include <algorithm>
#include <atomic>
#include <memory>
#include <stdexcept>

namespace Modules {

/*user recommended values*/
static const size_t ALLOC_NUM_BLOCKS_DEFAULT = 10;
static const size_t ALLOC_NUM_BLOCKS_LOW_LATENCY = 2;

auto const ALLOC_NUM_BLOCKS_MAX_DYN_FREE = 0 ;/*free the dynamically allocated blocks*/
//#define ALLOC_DEBUG_TRACK_BLOCKS

class PacketAllocator {
	public:
		PacketAllocator(size_t minBlocks, size_t maxBlocks) :
			minBlocks(minBlocks),
			maxBlocks(maxBlocks), curNumBlocks(minBlocks) {
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

		~PacketAllocator() {
			Block block;
			while (freeBlocks.tryPop(block)) {
				delete block.data;
			}
		}

		struct Deleter {
			Deleter(std::shared_ptr<PacketAllocator> allocator) : allocator(allocator) {}
			void operator()(IData *p) const {
				allocator->recycle(p);
			}
			std::shared_ptr<PacketAllocator> const allocator;
		};

		template<typename T>
		std::shared_ptr<T> getBuffer(size_t size, std::shared_ptr<PacketAllocator> allocator) {
			Block block;
			if (!freeBlocks.tryPop(block)) {
				if (curNumBlocks < maxBlocks) {
					freeBlocks.push(Block{});
					curNumBlocks++;
				}
				block = freeBlocks.pop();
			}
			switch (block.event) {
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
			freeBlocks.push(Block{Exit});
		}

	private:
		PacketAllocator& operator= (const PacketAllocator&) = delete;

		void recycle(IData *p) {
			if(ALLOC_NUM_BLOCKS_MAX_DYN_FREE) {
				if (curNumBlocks > minBlocks) {
					curNumBlocks--;
					delete p;
					return;
				}
			}
			if (!p->isRecyclable()) {
				delete p;
				p = nullptr;
			}
			freeBlocks.push(Block{OneBufferIsFree, p});
		}

		enum Event {
			OneBufferIsFree,
			Exit,
		};
		struct Block {
			Event event = OneBufferIsFree;
			IData*data = nullptr;
		};

		const size_t minBlocks;
		const size_t maxBlocks;
		std::atomic_size_t curNumBlocks;
		Queue<Block> freeBlocks;
#ifdef ALLOC_DEBUG_TRACK_BLOCKS
		Queue<std::weak_ptr<IData>> usedBlocks;
#endif
};

}
