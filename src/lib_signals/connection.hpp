#pragma once

namespace Signals {

template<typename ResultType, typename... Args>
class ConnectionList {
	public:
		IExecutor &executor;
		std::function<ResultType(Args...)> const callback;

		explicit ConnectionList(IExecutor &executor, const std::function<ResultType(Args...)> &callback) : executor(executor), callback(callback) {
		}
};

}
