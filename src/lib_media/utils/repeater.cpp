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
	addInput();
	addOutput<OutputDefault>();
	lastNow = high_resolution_clock::now();
	workingThread = std::thread(&Repeater::threadProc, this);
}

void Repeater::flush() {
	done = true;
	lastData = nullptr;
	if (workingThread.joinable()) {
		for (size_t i = 0; i < inputs.size(); ++i) {
			inputs[i]->push(nullptr);
		}
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
			if (lastData) outputs[0]->post(lastData);
			lastNow = high_resolution_clock::now();
		}
	}
}

void Repeater::process(Data data) {
	lastData = data;
	outputs[0]->post(lastData);
	lastNow = high_resolution_clock::now();
}

}
}
