#pragma once

#include "executor.hpp"
#include "connection.hpp"
#include <functional>
#include <map>
#include <mutex>

namespace Signals {

template<typename> class ISignal;

template <typename Callback, typename Arg>
class ISignal<Callback(Arg)> {
	public:
		virtual size_t connect(const std::function<Callback(Arg)> &cb, IExecutor<Callback(Arg)> &executor) = 0;
		virtual size_t connect(const std::function<Callback(Arg)> &cb) = 0;
		virtual bool disconnect(size_t connectionId) = 0;
		virtual size_t getNumConnections() const = 0;
		virtual size_t emit(Arg arg) = 0;
};

template<typename> class Signal;

template<typename Callback, typename Arg>
class Signal<Callback(Arg)> : public ISignal<Callback(Arg)> {
	private:
		typedef std::function<Callback(Arg)> CallbackType;
		typedef typename CallbackType::result_type ResultType;
		typedef ConnectionList<ResultType, Arg> ConnectionType;

	public:
		size_t connect(const CallbackType &cb, IExecutor<Callback(Arg)> &executor) {
			std::lock_guard<std::mutex> lg(callbacksMutex);
			const size_t connectionId = uid++;
			callbacks[connectionId] = std::make_unique<ConnectionType>(executor, cb, connectionId);
			return connectionId;
		}

		size_t connect(const CallbackType &cb) {
			return connect(cb, executor);
		}

		bool disconnect(size_t connectionId) {
			std::lock_guard<std::mutex> lg(callbacksMutex);
			return disconnectUnsafe(connectionId);
		}

		size_t getNumConnections() const {
			std::lock_guard<std::mutex> lg(callbacksMutex);
			return callbacks.size();
		}

		size_t emit(Arg arg) {
			std::lock_guard<std::mutex> lg(callbacksMutex);
			for (auto &cb : callbacks) {
				cb.second->futures.push_back(cb.second->executor(cb.second->callback, arg));
			}
			return callbacks.size();
		}

		Signal() : defaultExecutor(new ExecutorAsync<Callback(Arg)>()), executor(*defaultExecutor.get()) {
		}

		Signal(IExecutor<Callback(Arg)> &executor) : executor(executor) {
		}

		virtual ~Signal() {
		}

	private:
		Signal(const Signal&) = delete;
		Signal& operator= (const Signal&) = delete;

		bool disconnectUnsafe(size_t connectionId) {
			auto conn = callbacks.find(connectionId);
			if (conn == callbacks.end())
				return false;
			callbacks.erase(connectionId);
			return true;
		}

		mutable std::mutex callbacksMutex;
		std::map<size_t, std::unique_ptr<ConnectionType>> callbacks; //protected by callbacksMutex
		size_t uid = 0;                              //protected by callbacksMutex

		std::unique_ptr<IExecutor<Callback(Arg)>> const defaultExecutor;
		IExecutor<Callback(Arg)> &executor;
};

}
