#pragma once

#include "executor.hpp"
#include <functional>
#include <map>
#include <memory>
#include <mutex>

namespace Signals {

template<typename> struct ISignal;

template <typename Arg>
struct ISignal {
	virtual ~ISignal() = default;
	virtual int connect(const std::function<void(Arg)> &cb, IExecutor &executor) = 0;
	virtual int connect(const std::function<void(Arg)> &cb) = 0;
	virtual bool disconnect(int connectionId) = 0;
	virtual int getNumConnections() const = 0;
	virtual void emit(Arg arg) = 0;
};

template<typename> class Signal;

template<typename Arg>
class Signal : public ISignal<Arg> {
	private:
		typedef std::function<void(Arg)> CallbackType;

		struct ConnectionType {
			IExecutor* executor;
			std::function<void(Arg)> callback;
		};

	public:
		int connect(const CallbackType &cb, IExecutor &executor) {
			std::lock_guard<std::mutex> lg(callbacksMutex);
			const int connectionId = uid++;
			callbacks[connectionId] = {&executor, cb};
			return connectionId;
		}

		int connect(const CallbackType &cb) {
			return connect(cb, executor);
		}

		bool disconnect(int connectionId) {
			std::lock_guard<std::mutex> lg(callbacksMutex);
			return disconnectUnsafe(connectionId);
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
