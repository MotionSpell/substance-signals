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
