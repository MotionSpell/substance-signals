#pragma once

#include <functional>

namespace Signals {

struct IExecutor {
	virtual ~IExecutor() {}
	virtual void call(const std::function<void()> &fn) = 0;

	// make the executor stop accepting new tasks
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
