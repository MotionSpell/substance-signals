#pragma once

#include <functional>
#include "executor.hpp"

namespace Signals {

template<typename> struct ISignal;

template <typename Arg>
struct ISignal {
	virtual ~ISignal() = default;
	virtual int connect(const std::function<void(Arg)> &cb, IExecutor* executor = nullptr) = 0;
	virtual bool disconnect(int connectionId) = 0;
	virtual int getNumConnections() const = 0;
	virtual void emit(Arg arg) = 0;
};
}

