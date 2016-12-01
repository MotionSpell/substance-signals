#include "restamp.hpp"

namespace Modules {
namespace Transform {

Restamp::Restamp(Mode mode, int64_t offsetIn180k)
: offset(offsetIn180k), mode(mode) {
	addInput(new Input<DataBase>(this));
	addOutput<OutputDefault>();
}

Restamp::~Restamp() {
}

void Restamp::process(Data data) {
	uint64_t time;
	switch (mode) {
	case Passthru:
		time = data->getTime();
		break;
	case Reset:
		time = data->getTime();
		if (!isInitTime) {
			isInitTime = true;
			offset -= time;
		}
		break;
	case ClockSystem:
		time = g_DefaultClock->now();
		if (!isInitTime) {
			isInitTime = true;
			offset -= time;
		}
		break;
	case IgnoreFirstAndReset:
		assert(!isInitTime);
		time = 0;
		mode = Reset;
		break;
	default:
		throw error("Unknown mode");
	}

	if ((int64_t)(time + offset) < 0) {
		if (time < 2 * IClock::Rate) {
			log(Error, "CISCO: reset offset [%s -> %ss (time=%s, offset=%s)]", (double)data->getTime() / IClock::Rate, (double)(std::max<int64_t>(0, time + offset)) / IClock::Rate, time, offset);
			offset = 0;
		}
	}

	auto const restampedTime = std::max<int64_t>(0, time + offset);
	log(((time != 0) && ((int64_t)time + offset < 0)) ? Info : Debug, "%s -> %ss (time=%s, offset=%s)", (double)data->getTime() / IClock::Rate, (double)(restampedTime) / IClock::Rate, time, offset);
	const_cast<DataBase*>(data.get())->setTime(restampedTime); //FIXME: we should have input&output on the same allocator
	getOutput(0)->emit(data);
}

}
}
