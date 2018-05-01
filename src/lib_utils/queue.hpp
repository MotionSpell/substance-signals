#pragma once

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <type_traits>
#include <utility>

template<typename T>
class Queue {
public:
	Queue() {}
	virtual ~Queue() noexcept(false) {}

	void push(T data) {
		std::lock_guard<std::mutex> lock(mutex);
		pushUnsafe(data);
	}

	bool tryPop(T &value) {
		std::lock_guard<std::mutex> lock(mutex);
		if (dataQueue.empty()) {
			return false;
		}
		value = std::move(dataQueue.front());
		dataQueue.pop();
		return true;
	}

	T pop() {
		std::unique_lock<std::mutex> lock(mutex);
		while (dataQueue.empty())
			dataAvailable.wait(lock);
		T p;
		std::swap(p, dataQueue.front());
		dataQueue.pop();
		return p;
	}

	void clear() {
		std::lock_guard<std::mutex> lock(mutex);
		std::queue<T> emptyQueue;
		std::swap(emptyQueue, dataQueue);
	}

#ifdef TESTS
	size_t size() const {
		std::lock_guard<std::mutex> lock(mutex);
		return dataQueue.size();
	}

	T& operator[] (size_t index) {
		std::lock_guard<std::mutex> lock(mutex);
		const size_t dataSize = dataQueue.size();
		assert(index < dataSize);
		if (index == 0) {
			return dataQueue.front();
		}
		std::queue<T> tmpQueue;
		for (size_t i = 0; i < index; ++i) {
			tmpQueue.push(dataQueue.front());
			dataQueue.pop();
		}
		T &res = dataQueue.front();
		for (size_t i = index; i < dataSize; ++i) {
			tmpQueue.push(dataQueue.front());
			dataQueue.pop();
		}
		assert((dataQueue.size() == 0) && (tmpQueue.size() == dataSize));
		for (size_t i = 0; i < dataSize; ++i) {
			dataQueue.push(tmpQueue.front());
			tmpQueue.pop();
		}
		return res;
	}
#endif

protected:
	void pushUnsafe(T data) {
		dataQueue.push(std::move(data));
		dataAvailable.notify_one();
	}

	mutable std::mutex mutex;
	std::queue<T> dataQueue;
	std::condition_variable dataAvailable;

private:
	Queue(const Queue&) = delete;
	Queue& operator= (const Queue&) = delete;
};

/* Copyright 2016 Facebook, Inc., https://github.com/facebook/folly/blob/master/folly/ProducerConsumerQueue.h */
template<class T>
struct QueueLockFree {
	typedef T value_type;

	QueueLockFree(const QueueLockFree&) = delete;
	QueueLockFree& operator = (const QueueLockFree&) = delete;

	// size must be >= 2.
	//
	// Also, note that the number of usable slots in the queue at any
	// given time is actually (size-1), so if you start with an empty queue,
	// isFull() will return true after size-1 insertions.
	explicit QueueLockFree(uint32_t size)
		: size_(size)
		, records_(static_cast<T*>(std::malloc(sizeof(T) * size)))
		, readIndex_(0)
		, writeIndex_(0)
	{
		assert(size >= 2);
		if (!records_) {
			throw std::bad_alloc();
		}
	}

	~QueueLockFree() {
		// We need to destruct anything that may still exist in our queue.
		// (No real synchronization needed at destructor time: only one
		// thread can be doing this.)
		if (!std::is_trivially_destructible<T>::value) {
			size_t read = readIndex_;
			size_t end = writeIndex_;
			while (read != end) {
				records_[read].~T();
				if (++read == size_) {
					read = 0;
				}
			}
		}

		std::free(records_);
	}

	template<class ...Args>
	bool write(Args&&... recordArgs) {
		auto const currentWrite = writeIndex_.load(std::memory_order_relaxed);
		auto nextRecord = currentWrite + 1;
		if (nextRecord == size_) {
			nextRecord = 0;
		}
		if (nextRecord != readIndex_.load(std::memory_order_acquire)) {
			new (&records_[currentWrite]) T(std::forward<Args>(recordArgs)...);
			writeIndex_.store(nextRecord, std::memory_order_release);
			return true;
		}

		// queue is full
		return false;
	}

	// move (or copy) the value at the front of the queue to given variable
	bool read(T& record) {
		auto const currentRead = readIndex_.load(std::memory_order_relaxed);
		if (currentRead == writeIndex_.load(std::memory_order_acquire)) {
			// queue is empty
			return false;
		}

		auto nextRecord = currentRead + 1;
		if (nextRecord == size_) {
			nextRecord = 0;
		}
		record = std::move(records_[currentRead]);
		records_[currentRead].~T();
		readIndex_.store(nextRecord, std::memory_order_release);
		return true;
	}

	// pointer to the value at the front of the queue (for use in-place) or
	// nullptr if empty.
	T* frontPtr() {
		auto const currentRead = readIndex_.load(std::memory_order_relaxed);
		if (currentRead == writeIndex_.load(std::memory_order_acquire)) {
			// queue is empty
			return nullptr;
		}
		return &records_[currentRead];
	}

	// queue must not be empty
	void popFront() {
		auto const currentRead = readIndex_.load(std::memory_order_relaxed);
		assert(currentRead != writeIndex_.load(std::memory_order_acquire));

		auto nextRecord = currentRead + 1;
		if (nextRecord == size_) {
			nextRecord = 0;
		}
		records_[currentRead].~T();
		readIndex_.store(nextRecord, std::memory_order_release);
	}

	bool isEmpty() const {
		return readIndex_.load(std::memory_order_acquire) ==
			writeIndex_.load(std::memory_order_acquire);
	}

	bool isFull() const {
		auto nextRecord = writeIndex_.load(std::memory_order_acquire) + 1;
		if (nextRecord == size_) {
			nextRecord = 0;
		}
		if (nextRecord != readIndex_.load(std::memory_order_acquire)) {
			return false;
		}
		// queue is full
		return true;
	}

	// * If called by consumer, then true size may be more (because producer may
	//   be adding items concurrently).
	// * If called by producer, then true size may be less (because consumer may
	//   be removing items concurrently).
	// * It is undefined to call this from any other thread.
	size_t sizeGuess() const {
		int ret = writeIndex_.load(std::memory_order_acquire) -
			readIndex_.load(std::memory_order_acquire);
		if (ret < 0) {
			ret += size_;
		}
		return ret;
	}

private:
	const uint32_t size_;
	T* const records_;

	std::atomic<unsigned int> readIndex_;
	std::atomic<unsigned int> writeIndex_;
};
