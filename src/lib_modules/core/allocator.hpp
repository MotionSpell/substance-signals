#pragma once

#include "data.hpp"
#include "lib_signals/utils/queue.hpp"
#include <algorithm>
#include <atomic>
#include <list>
#include <memory>
#include <stdexcept>

namespace Modules {

/*user recommended values*/
static const size_t ALLOC_NUM_BLOCKS_DEFAULT = 10;
static const size_t ALLOC_NUM_BLOCKS_LOW_LATENCY = 2;

//#define ALLOC_NUM_BLOCKS_MAX_DYN (std::min<size_t>(numBlocks+3, numBlocks*2)) /*flexibility on the allocator*/
#ifdef ALLOC_NUM_BLOCKS_MAX_DYN
  #define ALLOC_NUM_BLOCKS_MAX_DYN_FREE /*free the dynamically allocated blocks*/
#endif /*ALLOC_NUM_BLOCKS_MAX_DYN*/

//#define ALLOC_DEBUG_TRACK_BLOCKS

template<typename DataType>
class PacketAllocator {
	public:
		typedef DataType MyType;
		PacketAllocator(size_t numBlocks)
#ifdef ALLOC_NUM_BLOCKS_MAX_DYN
			:
#ifdef ALLOC_NUM_BLOCKS_MAX_DYN_FREE
			targetNumBlocks(numBlocks),
#endif
			maxDynNumBlocks(ALLOC_NUM_BLOCKS_MAX_DYN), curNumBlocks(numBlocks)
#endif
		{
			if (numBlocks == 0)
				throw std::runtime_error("Cannot create an allocator with 0 block.");
			for (size_t i=0; i<numBlocks; ++i) {
				freeBlocks.push(Block());
			}
		}

		~PacketAllocator() {
			Block block;
			while (freeBlocks.tryPop(block)) {
				if (block.event == OneBufferIsFree) {
					delete block.data;
				}
			}
		}

		struct Deleter {
			Deleter(PacketAllocator<DataType> *allocator) : allocator(allocator) {}
			void operator()(DataType *p) const {
				allocator->recycle(p);
			}
			PacketAllocator<DataType> * const allocator;
		};

		template<typename T>
		std::shared_ptr<T> getBuffer(size_t size) {
			Block block;
			if (!freeBlocks.tryPop(block)) {
#ifdef ALLOC_NUM_BLOCKS_MAX_DYN
				if (curNumBlocks < maxDynNumBlocks) {
					freeBlocks.push(Block(OneBufferIsFree));
					curNumBlocks++;
				}
#endif
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

				auto ret = std::shared_ptr<T>(safe_cast<T>(block.data), Deleter(this));
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
			freeBlocks.push(Block(Exit));
		}

	private:
		PacketAllocator& operator= (const PacketAllocator&) = delete;
		void recycle(DataType *p) {
#ifdef ALLOC_NUM_BLOCKS_MAX_DYN_FREE
			if (curNumBlocks > targetNumBlocks) {
				curNumBlocks--;
				delete p;
				return;
			}
#endif
			if (!p->isRecyclable()) {
				delete p;
				p = nullptr;
			}
			freeBlocks.push(Block(OneBufferIsFree, p));
		}

		enum Event {
			OneBufferIsFree,
			Exit,
		};
		struct Block {
			Block(Event event = OneBufferIsFree, DataType *data = nullptr): event(event), data(data) {}
			Event event;
			DataType *data;
		};

#ifdef ALLOC_NUM_BLOCKS_MAX_DYN
#ifdef ALLOC_NUM_BLOCKS_MAX_DYN_FREE
		const size_t targetNumBlocks;
#endif
		const size_t maxDynNumBlocks;
		std::atomic_size_t curNumBlocks;
#endif
		Signals::Queue<Block> freeBlocks;
#ifdef ALLOC_DEBUG_TRACK_BLOCKS
		Signals::Queue<std::weak_ptr<DataType>> usedBlocks;
#endif
};

}
