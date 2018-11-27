#pragma once

#include "queue.hpp"
#include <functional>
#include <stdexcept>
#include <thread>
#include <cassert>

class ThreadPool {
	public:
		ThreadPool(const std::string &name = "", int threadCount = std::thread::hardware_concurrency())
			: name(name) {
			for (int i = 0; i < threadCount; ++i) {
				threads.push_back(std::thread(&ThreadPool::run, this));
			}
		}

		~ThreadPool() {
			workQueue.clear(); // speedup exit
			for(auto& t : threads) {
				(void)t;
				workQueue.push(nullptr);
			}
			for(auto& t : threads) {
				t.join();
			}
		}

		void submit(std::function<void()> f)	{
			if (eptr)
				std::rethrow_exception(eptr);

			assert(f);

			workQueue.push(f);
		}

	private:
		ThreadPool(const ThreadPool&) = delete;

		void run() {
			while (auto task = workQueue.pop()) {
				try {
					task();
				} catch (...) {
					eptr = std::current_exception(); //will be caught by next submit()
					// FIXME: shouldn't 'eptr' be protected from concurrent read/write?
					// FIXME: what if there's no next submit? is the exception lost?
				}
			}
		}

		Queue<std::function<void(void)>> workQueue;
		std::vector<std::thread> threads;
		std::string name;
		std::exception_ptr eptr;
};
