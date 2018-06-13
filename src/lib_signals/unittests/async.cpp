#include "tests/tests.hpp"
#include "lib_signals/signals.hpp"
#include "lib_utils/queue_inspect.hpp" // transferToVector

using namespace Tests;
using namespace Signals;

namespace {

inline int dummy(int a) {
	return a;
}
inline void sleepInMs(int ms) {
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

secondclasstest("destroy on execution") {
	Signal<void(int)> sig;
	sig.connect(sleepInMs);
	sig.emit(1000);
}

secondclasstest("disconnect before execution") {
	Signal<void(int)> sig;
	size_t uid = sig.connect(sleepInMs);
	sig.disconnect(uid);
	sig.emit(1000);
}

secondclasstest("disconnect on execution") {
	Signal<void(int)> sig;
	size_t uid = sig.connect(sleepInMs);
	sig.emit(1000);
	sig.disconnect(uid);
}
}

