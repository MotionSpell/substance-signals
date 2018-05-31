#pragma once

namespace Signals {

/* member function helper */
template<typename Result, typename Class, typename MemberFunction>
class MemberFunctor {
	public:
		MemberFunctor(Class *object, MemberFunction function) : object(object), function(function) {
		}

		template<typename... Args>
		Result operator()(Args... args) {
			return (object->*function)(args...);
		}

	private:
		Class *object;
		MemberFunction function;
};

template<typename Result, typename Class, typename... Args>
MemberFunctor<Result, Class, Result (Class::*)(Args...)>
BindMember(Class* objectPtr, Result (Class::*memberFunction) (Args...)) {
	return MemberFunctor<Result, Class, Result (Class::*)(Args...)>(objectPtr, memberFunction);
}

template<typename SignalType, typename LambdaType, typename Executor>
size_t Connect(SignalType& sig, LambdaType lambda, Executor& executor) {
	return sig.connect(lambda, executor);
}

template<typename SignalType, typename LambdaType>
size_t Connect(SignalType& sig, LambdaType lambda) {
	return sig.connect(lambda);
}

}
