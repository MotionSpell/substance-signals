#pragma once

#include "queue.hpp"
#include <atomic>
#include <functional>
#include <future>
#include <stdexcept>
#include <thread>

class ThreadPool {
	public:
		ThreadPool(const std::string &name = "", int threadCount = std::thread::hardware_concurrency())
			: name(name) {
			waitAndExit = false;
			for (int i = 0; i < threadCount; ++i) {
				threads.push_back(std::thread(&ThreadPool::run, this));
			}
		}

		~ThreadPool() {
			waitAndExit = true;
			workQueue.clear(); // speedup exit
			for(auto& t : threads) {
				(void)t;
				workQueue.push([] {});
			}
			for(auto& t : threads) {
				if (t.joinable()) {
					t.join();
				}
			}
		}

		template<typename Callback, typename... Args>
		std::shared_future<Callback> submit(const std::function<Callback(Args...)> &callback, Args... args)	{
			if (eptr)
				std::rethrow_exception(eptr);

			const std::shared_future<Callback> &future = std::async(std::launch::deferred, callback, args...);
			std::function<void(void)> f = [future]() {
				future.get();
			};
			workQueue.push(f);
			return future;
		}

	private:
		ThreadPool(const ThreadPool&) = delete;

		void run() {
			while (!waitAndExit) {
				auto task = workQueue.pop();
				try {
					task();
				} catch (...) {
					eptr = std::current_exception(); //will be caught by next submit()
				}
			}
		}

		std::atomic_bool waitAndExit;
		Queue<std::function<void(void)>> workQueue;
		std::vector<std::thread> threads;
		std::string name;
		std::exception_ptr eptr;
};
