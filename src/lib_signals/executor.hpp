#pragma once

#include <functional>

namespace Signals {

struct IExecutor {
	virtual ~IExecutor() noexcept(false) {}
	virtual void operator() (const std::function<void()> &fn) = 0;
};

//synchronous calls
class ExecutorSync : public IExecutor {
	public:
		void operator() (const std::function<void()> &fn) {
			fn();
		}
};
}

// TODO: move this elsewhere
#include <future>

namespace Signals {

//asynchronous calls with std::launch::async (spawns a thread)
class ExecutorAsync : public IExecutor {
	public:
		void operator() (const std::function<void()> &fn) {
			std::async(std::launch::async, fn);
		}
};

}
