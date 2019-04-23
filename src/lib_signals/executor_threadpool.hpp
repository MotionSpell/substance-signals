#pragma once

#include "executor.hpp"
#include "lib_utils/threadpool.hpp"

namespace Signals {

//tasks occur in a thread
class ExecutorThread : public IExecutor {
	public:
		ExecutorThread(const std::string &name) : threadPool(std::make_unique<ThreadPool>(name, 1)) {
		}

		void call(const std::function<void()> &fn) override {
			std::unique_lock<std::mutex> lock(mutex);
			if(threadPool)
				threadPool->submit(fn);
		}

		void kill() override {

			std::unique_ptr<ThreadPool> pool;

			// atomically set 'threadPool' to null, but without destroying it
			{
				std::unique_lock<std::mutex> lock(mutex);
				pool = std::move(threadPool);
			}

			// wait for all threads to be stopped,
			// but don't hold the mutex
			pool.reset();
		}

	private:
		std::mutex mutex;
		std::unique_ptr<ThreadPool> threadPool;
};

}
