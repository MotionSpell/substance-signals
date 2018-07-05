#pragma once

#include <future>
#include <vector>

namespace Signals {

template<typename ResultType, typename... Args>
class ConnectionList {
	public:
		typedef ResultType FutureResultType;
		IExecutor<ResultType(Args...)> &executor;
		std::function<ResultType(Args...)> const callback;
		size_t const uid;

		explicit ConnectionList(IExecutor<ResultType(Args...)> &executor, const std::function<ResultType(Args...)> &callback, const size_t uid) : executor(executor), callback(callback), uid(uid) {
		}
};

template<typename... Args>
class ConnectionList<void, Args...> {
	private:
		template<typename T>
		class FakeVector {
			public:
				void push_back(T const&) {}
				T* erase(T*) {
					return nullptr;
				}
				T* begin() {
					return nullptr;
				}
				T* end() {
					return nullptr;
				}
		};

	public:
		IExecutor<void(Args...)> &executor;
		std::function<void(Args...)> const callback;
		size_t const uid;
		FakeVector<std::shared_future<void>> futures;

		explicit ConnectionList(IExecutor<void(Args...)> &executor, const std::function<void(Args...)> &callback, const size_t uid) : executor(executor), callback(callback), uid(uid) {
		}
};

}
