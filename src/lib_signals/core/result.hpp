#pragma once

#include <vector>
#include <memory>
#include "lib_utils/queue.hpp"

namespace Signals {

template<typename ResultType>
class ResultQueue {
	public:
		typedef std::shared_ptr<Queue<ResultType>> ResultValue;

		explicit ResultQueue() : results(new Queue<ResultType>()) {}
		virtual ~ResultQueue() {}

		void set(ResultType r) {
			results->push(r);
		}

		ResultValue& get() {
			return results;
		}

		void clear() {
			results->clear();
		}

	private:
		ResultValue results;
};

//specialized for void
template<>
class ResultQueue<void> {
	public:
		typedef std::shared_ptr<void> ResultValue;

		explicit ResultQueue() {}
		virtual ~ResultQueue() {}

		void set(int) {
		}

		std::shared_ptr<void> get() {
			return std::shared_ptr<void>();
		}

		void clear() {
		}
};

template<typename ResultType>
class ResultVector  {
	public:
		typedef std::shared_ptr<std::vector<ResultType>> ResultValue;

		explicit ResultVector() : results(new std::vector<ResultType>()) {}
		virtual ~ResultVector() {}

		void set(ResultType r) {
			results->push_back(r);
		}

		ResultValue& get() {
			return results;
		}

		void clear() {
			results->clear();
		}

	private:
		ResultValue results;
};

//specialized for void
template<>
class ResultVector<void>  {
	public:
		typedef std::shared_ptr<void> ResultValue;

		explicit ResultVector()  {}
		virtual ~ResultVector() {}

		void set(int) {
		}

		std::shared_ptr<void> get() {
			return std::shared_ptr<void>();
		}

		void clear() {
		}
};

}
