#pragma once

#include "executor.hpp"
#include "lib_utils/threadpool.hpp"

namespace Signals {

//tasks occur in a thread
class ExecutorThread : public IExecutor {
	public:
		ExecutorThread(const std::string &name) : m_threadPool(name, 1) {
			killed = false;
		}

		void call(const std::function<void()> &fn) override {
			if(killed)
				return;
			m_threadPool.submit(fn);
		}

		void kill() override {
			// don't destroy the ThreadPool here, as doing so might block.
			killed = true;
		}

	private:
		std::atomic<bool> killed;
		ThreadPool m_threadPool;
};

}
