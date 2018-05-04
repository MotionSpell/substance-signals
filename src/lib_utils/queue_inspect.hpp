#pragma once

#include <vector>
#include "queue.hpp"

template<typename T>
std::vector<T> transferToVector(Queue<T>& q) {
	std::vector<T> r;
	T val;
	while(q.tryPop(val))
		r.push_back(val);
	return r;
}

