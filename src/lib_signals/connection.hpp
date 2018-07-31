#pragma once

namespace Signals {

template<typename... Args>
class ConnectionList {
	public:
		IExecutor &executor;
		std::function<void(Args...)> const callback;

		explicit ConnectionList(IExecutor &executor, const std::function<void(Args...)> &callback) : executor(executor), callback(callback) {
		}
};

}
