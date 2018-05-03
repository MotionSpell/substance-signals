#pragma once

#include "queue.hpp"
#include <atomic>
#include <functional>
#include <future>
#include <stdexcept>
#include <thread>

class ThreadPool {
	public:
		ThreadPool(const std::string &name = "", unsigned threadCount = std::thread::hardware_concurrency())
			: name(name) {
			done = false;
			waitAndExit = false;
			for (unsigned i = 0; i < threadCount; ++i) {
				threads.push_back(std::thread(&ThreadPool::run, this));
			}
		}

		~ThreadPool() {
			WaitForCompletion();
			done = true;
		}

		void WaitForCompletion() {
			waitAndExit = true;
			for (size_t i = 0; i < threads.size(); ++i) {
				workQueue.push([] {});
			}
			for (size_t i = 0; i < threads.size(); ++i) {
				if (threads[i].joinable()) {
					threads[i].join();
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
			while (!done) {
				std::function<void(void)> task = workQueue.pop();
				try {
					task();
				} catch (...) {
					eptr = std::current_exception(); //will be caught by next submit()
				}
				if (waitAndExit) {
					done = true;
				}
			}
		}

		std::atomic_bool done, waitAndExit;
		Queue<std::function<void(void)>> workQueue;
		std::vector<std::thread> threads;
		std::string name;
		std::exception_ptr eptr;
};
