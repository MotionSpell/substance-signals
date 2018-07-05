#pragma once

#include <functional>

namespace Signals {

template<typename> class IExecutor;

template <typename... Args>
class IExecutor<void(Args...)> {
	public:
		virtual ~IExecutor() noexcept(false) {}
		virtual void operator() (const std::function<void(Args...)> &fn, Args... args) = 0;
};

template<typename> class ExecutorSync;
template<typename> class ExecutorAsync;

//synchronous calls
template<typename... Args>
class ExecutorSync<void(Args...)> : public IExecutor<void(Args...)> {
	public:
		void operator() (const std::function<void(Args...)> &fn, Args... args) {
			fn(args...);
		}
};
}

// TODO: move this elsewhere
#include <future>

namespace Signals {

//asynchronous calls with std::launch::async (spawns a thread)
template< typename... Args>
class ExecutorAsync<void(Args...)> : public IExecutor<void(Args...)> {
	public:
		void operator() (const std::function<void(Args...)> &fn, Args... args) {
			std::async(std::launch::async, fn, args...);
		}
};

}
