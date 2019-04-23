#pragma once

#include <functional>

namespace Signals {

struct IExecutor {
	virtual ~IExecutor() {}
	virtual void call(const std::function<void()> &fn) = 0;

	// tells the executor to stop executing anything.
	// This call should be synchronous, i.e on return, nothing should be
	// running anymore in this executor, and all subsequents calls to 'call'
	// will be ignored.
	virtual void kill() {};
};

//synchronous calls
class ExecutorSync : public IExecutor {
	public:
		void call(const std::function<void()> &fn) override {
			if(killed)
				return;
			fn();
		}

		void kill() override {
			killed = true;
		}
	private:
		bool killed = false;
};
}
