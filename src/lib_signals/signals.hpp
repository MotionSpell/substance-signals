#pragma once

#include "helper.hpp"
#include "signal.hpp"

#include <map>
#include <memory>
#include <mutex>

namespace Signals {

template<typename Arg>
class Signal : public ISignal<Arg> {
	public:
		typedef std::function<void(Arg)> CallbackType;

		int connect(const CallbackType &cb, IExecutor* executor = nullptr) {
			if(!executor)
				executor = &this->executor;
			std::lock_guard<std::mutex> lg(callbacksMutex);
			const int connectionId = uid++;
			callbacks[connectionId] = {executor, cb};
			return connectionId;
		}

		void disconnect(int connectionId) {
			std::lock_guard<std::mutex> lg(callbacksMutex);
			disconnectUnsafe(connectionId);
		}

		// FIXME: in a concurrent context, what's returned by this function
		// immediately becomes obsolete.
		int getNumConnections() const {
			std::lock_guard<std::mutex> lg(callbacksMutex);
			return (int)callbacks.size();
		}

		void emit(Arg arg) {
			std::lock_guard<std::mutex> lg(callbacksMutex);
			for (auto &cb : callbacks) {
				(*cb.second.executor)(std::bind(cb.second.callback, arg));
			}
		}

		Signal() : defaultExecutor(new ExecutorSync()), executor(*defaultExecutor.get()) {
		}

		Signal(IExecutor &executor) : executor(executor) {
		}

	private:
		Signal(const Signal&) = delete;
		Signal& operator= (const Signal&) = delete;

		struct ConnectionType {
			IExecutor* executor;
			std::function<void(Arg)> callback;
		};

		bool disconnectUnsafe(int connectionId) {
			auto conn = callbacks.find(connectionId);
			if (conn == callbacks.end())
				return false;
			callbacks.erase(connectionId);
			return true;
		}

		mutable std::mutex callbacksMutex;
		std::map<int, ConnectionType> callbacks;  //protected by callbacksMutex
		int uid = 0;                              //protected by callbacksMutex

		std::unique_ptr<IExecutor> const defaultExecutor;
		IExecutor &executor;
};

}
