#include "lib_utils/tools.hpp"
#include "repeater.hpp"
#include <algorithm> //std::max

namespace Modules {
namespace Utils {

const int64_t maxTimeInMs = 500;

using namespace std::chrono;

Repeater::Repeater(KHost* host, int64_t ms)
	: m_host(host), periodInMs(ms) {
	(void)m_host;
	done = false;
	m_output = addOutput<OutputDefault>();
	lastNow = high_resolution_clock::now();
	workingThread = std::thread(&Repeater::threadProc, this);
}

void Repeater::flush() {
	done = true;
	lastData = nullptr;
	if (workingThread.joinable()) {
		safe_cast<IInput>(input)->push(nullptr);
		workingThread.join();
	}
}

Repeater::~Repeater() {
	flush();
}

void Repeater::threadProc() {
	while (!done) {
		std::this_thread::sleep_for(milliseconds(std::min(periodInMs, maxTimeInMs)));
		auto const now = high_resolution_clock::now();
		auto const waitTimeInMs = (duration_cast<milliseconds>(now - lastNow)).count();
		if (waitTimeInMs > periodInMs) {
			if (lastData) m_output->post(lastData);
			lastNow = high_resolution_clock::now();
		}
	}
}

void Repeater::processOne(Data data) {
	lastData = data;
	m_output->post(lastData);
	lastNow = high_resolution_clock::now();
}

}
}
