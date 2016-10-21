#pragma once

#include "executor.hpp"
#include "connection.hpp"
#include "result.hpp"

#include <cassert>
#include <chrono>
#include <functional>
#include <map>
#include <mutex>


namespace Signals {

template<typename> class ISignal;

template <typename Callback, typename... Args>
class ISignal<Callback(Args...)> {
	public:
		virtual size_t connect(const std::function<Callback(Args...)> &cb, IExecutor<Callback(Args...)> &executor) = 0;
		virtual size_t connect(const std::function<Callback(Args...)> &cb) = 0;
		virtual bool disconnect(size_t connectionId) = 0;
		virtual IExecutor<Callback(Args...)>& getExecutor() const = 0;
		virtual size_t getNumConnections() const = 0;
		virtual size_t emit(Args... args) = 0;
		virtual void sync() = 0; /*wait for all running operations to end - safe to concurrent calls*/
};

/**
 * The PSignal class exists because we cannot pass default parameters and packed argument for the same class
 */
template<typename, typename> class PSignal;

template<typename Result, typename Callback, typename... Args>
class PSignal<Result, Callback(Args...)> : public ISignal<Callback(Args...)> {
	protected:
		typedef std::function<Callback(Args...)> CallbackType;

	private:
		typedef typename Result::ResultValue ResultValue;
		typedef typename CallbackType::result_type ResultType;
		typedef ConnectionList<ResultType, Args...> ConnectionType;
		typedef std::map<size_t, ConnectionType*> ConnectionManager;

	public:
		size_t connect(const CallbackType &cb, IExecutor<Callback(Args...)> &executor) {
			std::lock_guard<std::mutex> lg(callbacksMutex);
			const size_t connectionId = uid++;
			callbacks[connectionId] = new ConnectionType(executor, cb, connectionId);
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

		size_t emit(Args... args) {
			std::lock_guard<std::mutex> lg(callbacksMutex);
			result.clear();
			for (auto &cb : callbacks) {
				cb.second->futures.push_back(cb.second->executor(cb.second->callback, args...));
			}
			return callbacks.size();
		}

		void sync() {
			std::lock_guard<std::mutex> lg(callbacksMutex);
			fillResultsUnsafe();
		}

		ResultValue results(bool sync = true, bool single = false) {
			std::lock_guard<std::mutex> lg(callbacksMutex);
			fillResultsUnsafe(sync, single);
			return result.get();
		}

		IExecutor<Callback(Args...)>& getExecutor() const {
			return executor;
		}

	protected:
		PSignal() : defaultExecutor(new ExecutorAsync<Callback(Args...)>()), executor(*defaultExecutor.get()) {
		}

		PSignal(IExecutor<Callback(Args...)> &executor) : executor(executor) {
		}

		virtual ~PSignal() {
			while (!callbacks.empty()) { //delete still connected callbacks
				auto& cb = *callbacks.begin();
				bool res = disconnectUnsafe(cb.first);
				assert(res);
			}
		}

	private:
		PSignal(const PSignal&) = delete;
		PSignal& operator= (const PSignal&) = delete;

		bool disconnectUnsafe(size_t connectionId) {
			auto conn = callbacks.find(connectionId);
			if (conn == callbacks.end())
				return false;
			delete conn->second;
			callbacks.erase(connectionId);
			return true;
		}

		void fillResultsUnsafe(bool sync = true, bool single = false) {
			for (auto &cb : callbacks) {
				for (auto f = cb.second->futures.begin(); f != cb.second->futures.end();) {
					if (!sync && (f->wait_for(std::chrono::nanoseconds(0)) != std::future_status::ready)) {
						++f;
					} else {
						assert(f->valid());
						result.set(f->get());
						f = cb.second->futures.erase(f);
						if (single) {
							return;
						}
					}
				}
			}
		}

		mutable std::mutex callbacksMutex;
		ConnectionManager callbacks; //protected by callbacksMutex
		Result result;               //protected by callbacksMutex
		size_t uid = 0;              //protected by callbacksMutex

		std::unique_ptr<IExecutor<Callback(Args...)>> const defaultExecutor;
		IExecutor<Callback(Args...)> &executor;
};

template <typename SignalSignature, typename Result = ResultQueue<typename std::function<SignalSignature>::result_type >>
class Signal : public PSignal<Result, SignalSignature> {
	public:
		Signal() : PSignal<Result, SignalSignature>() {}
		Signal(IExecutor<SignalSignature> &executor) : PSignal<Result, SignalSignature>(executor) {}
};

}
