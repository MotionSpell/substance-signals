#pragma once

#include "executor.hpp"
#include "lib_utils/threadpool.hpp"

namespace Signals {

//tasks occur in the pool
class ExecutorThreadPool : public IExecutor {
	public:
		ExecutorThreadPool() : threadPool(std::make_shared<ThreadPool>()) {
		}

		ExecutorThreadPool(std::shared_ptr<ThreadPool> threadPool) : threadPool(threadPool) {
		}

		void operator() (const std::function<void()> &fn) {
			threadPool->submit(fn);
		}

	private:
		std::shared_ptr<ThreadPool> threadPool;
};

//tasks occur in a thread
class ExecutorThread : public IExecutor {
	public:
		ExecutorThread(const std::string &name) : threadPool(name, 1) {
		}

		void operator() (const std::function<void()> &fn) {
			threadPool.submit(fn);
		}

	private:
		ThreadPool threadPool;
};

}
