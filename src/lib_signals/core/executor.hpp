#pragma once

#include "lib_utils/threadpool.hpp"
#include "lib_utils/tools.hpp"
#include <functional>
#include <future>

namespace Signals {

template<typename> class IExecutor;

template <typename... Args>
class IExecutor<void(Args...)> {
	public:
		virtual ~IExecutor() noexcept(false) {}
		virtual std::shared_future<void> operator() (const std::function<void(Args...)> &fn, Args... args) = 0;
};

template<typename> class ExecutorSync;
template<typename> class ExecutorLazy;
template<typename> class ExecutorAsync;
template<typename> class ExecutorAuto;
template<typename> class ExecutorThread;
template<typename> class ExecutorThreadPool;

//synchronous calls
template<typename... Args>
class ExecutorSync<void(Args...)> : public IExecutor<void(Args...)> {
	public:
		std::shared_future<void> operator() (const std::function<void(Args...)> &fn, Args... args) {
			std::packaged_task<void(Args...)> task(fn);
			const std::shared_future<void> &f = task.get_future();
			task(args...);
			f.get();
			return f;
		}
};

//synchronous lazy calls
template<typename... Args>
class ExecutorLazy<void(Args...)> : public IExecutor<void(Args...)> {
	public:
		std::shared_future<void> operator() (const std::function<void(Args...)> &fn, Args... args) {
			return std::async(std::launch::deferred, fn, args...);
		}
};

//asynchronous calls with std::launch::async (spawns a thread)
template< typename... Args>
class ExecutorAsync<void(Args...)> : public IExecutor<void(Args...)> {
	public:
		std::shared_future<void> operator() (const std::function<void(Args...)> &fn, Args... args) {
			return std::async(std::launch::async, fn, args...);
		}
};

//asynchronous or synchronous calls at the runtime convenience
template< typename... Args>
class ExecutorAuto<void(Args...)> : public IExecutor<void(Args...)> {
	public:
		std::shared_future<void> operator() (const std::function<void(Args...)> &fn, Args... args) {
			return std::async(std::launch::async | std::launch::deferred, fn, args...);
		}
};

//tasks occur in a thread
template< typename... Args>
class ExecutorThread<void(Args...)> : public IExecutor<void(Args...)> {
	public:
		ExecutorThread(const std::string &name) : threadPool(name, 1) {
		}

		std::shared_future<void> operator() (const std::function<void(Args...)> &fn, Args... args) {
			return threadPool.submit(fn, args...);
		}

	private:
		ThreadPool threadPool;
};

//tasks occur in the pool
template< typename... Args>
class ExecutorThreadPool<void(Args...)> : public IExecutor<void(Args...)> {
	public:
		ExecutorThreadPool() : threadPool(make_shared<ThreadPool>()) {
		}

		ExecutorThreadPool(std::shared_ptr<ThreadPool> threadPool) : threadPool(threadPool) {
		}

		std::shared_future<void> operator() (const std::function<void(Args...)> &fn, Args... args) {
			return threadPool->submit(fn, args...);
		}

	private:
		std::shared_ptr<ThreadPool> threadPool;
};

}
