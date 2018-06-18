#pragma once

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
template<typename> class ExecutorAsync;

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

//asynchronous calls with std::launch::async (spawns a thread)
template< typename... Args>
class ExecutorAsync<void(Args...)> : public IExecutor<void(Args...)> {
	public:
		std::shared_future<void> operator() (const std::function<void(Args...)> &fn, Args... args) {
			return std::async(std::launch::async, fn, args...);
		}
};

}
