#pragma once

#include "executor.hpp"
#include "lib_utils/threadpool.hpp"

namespace Signals {

//tasks occur in a thread
class ExecutorThread : public IExecutor {
	public:
		ExecutorThread(const std::string &name) : threadPool(name, 1) {
		}

		void call(const std::function<void()> &fn) override {
			threadPool.submit(fn);
		}

	private:
		ThreadPool threadPool;
};

}
