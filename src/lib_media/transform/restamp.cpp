#include "restamp.hpp"
#include "lib_utils/system_clock.hpp"

namespace Modules {
namespace Transform {

Restamp::Restamp(IModuleHost* host, Mode mode, int64_t offsetIn180k)
	: m_host(host), offset(offsetIn180k), mode(mode) {
	addInput(this);
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
		time = fractionToClock(g_SystemClock->now());
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
			m_host->log(Error, format("reset offset [%s -> %ss (time=%s, offset=%s)]", (double)time / IClock::Rate, (double)(std::max<int64_t>(0, time + offset)) / IClock::Rate, time, offset).c_str());
			offset = 0;
		}
	}

	return std::max<int64_t>(0, time + offset);
}

void Restamp::process(Data data) {
	auto const time = data->getMediaTime();
	auto const restampedTime = restamp(time);
	m_host->log(((time != 0) && (time + offset < 0)) ? Info : Debug, format("%s -> %ss (time=%s, offset=%s)", (double)time / IClock::Rate, (double)(restampedTime) / IClock::Rate, time, offset).c_str());
	auto dataOut = make_shared<DataBaseRef>(data);
	dataOut->setMediaTime(restampedTime);
	getOutput(0)->emit(dataOut);
}

}
}
