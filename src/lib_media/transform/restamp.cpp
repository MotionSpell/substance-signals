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

int64_t Restamp::restamp(int64_t time) {
	switch (mode) {
	case Passthru:
		break;
	case Reset:
		if (!isInitTime) {
			isInitTime = true;
			offset -= time;
		}
		break;
	case ClockSystem:
		time = fractionToClock(g_DefaultClock->now());
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

	if (time + offset < 0) {
		if (time / IClock::Rate < 2) {
			log(Error, "reset offset [%s -> %ss (time=%s, offset=%s)]", (double)time / IClock::Rate, (double)(std::max<int64_t>(0, time + offset)) / IClock::Rate, time, offset);
			offset = 0;
		}
	}

	return std::max<int64_t>(0, time + offset);
}

void Restamp::process(Data data) {
	auto const time = data->getMediaTime();
	auto const restampedTime = restamp(time);
	log(((time != 0) && (time + offset < 0)) ? Info : Debug, "%s -> %ss (time=%s, offset=%s)", (double)time / IClock::Rate, (double)(restampedTime) / IClock::Rate, time, offset);
	auto dataOut = std::make_shared<DataBaseRef>(data);
	dataOut->setMediaTime(restampedTime);
	getOutput(0)->emit(dataOut);
}

}
}
