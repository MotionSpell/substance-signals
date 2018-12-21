#include "tests/tests.hpp"
#include "lib_signals/signals.hpp"
#include "lib_utils/tools.hpp" // makeVector
#include "lib_utils/queue_inspect.hpp"

using namespace Tests;
using namespace Signals;

namespace {
struct Signaler {
	Signal<int> signal;
};

inline int dummyPrint(int a) {
	return a;
}

struct Slot {
	std::vector<int> vals;
	void slot(int a) {
		vals.push_back(1 + dummyPrint(a));
	}
};

unittest("basic module connection tests") {
	Signaler sender;
	Slot receiver;
	Slot &receiverRef = receiver;
	Slot *receiverPtr = &receiver;
	sender.signal.connect(BindMember(&receiver, &Slot::slot));
	sender.signal.connect(BindMember(&receiver, &Slot::slot));
	sender.signal.connect(BindMember(&receiver, &Slot::slot));
	sender.signal.connect(BindMember(&receiverRef, &Slot::slot));
	sender.signal.connect(BindMember(receiverPtr, &Slot::slot));

	sender.signal.emit(100);
	ASSERT_EQUALS(makeVector({101, 101, 101, 101, 101}), receiver.vals);
}
}
