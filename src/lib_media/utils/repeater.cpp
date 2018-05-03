#include "lib_utils/tools.hpp"
#include "repeater.hpp"

namespace Modules {
namespace Utils {

using namespace std::chrono;

Repeater::Repeater(int64_t ms) : timeInMs(ms) {
	done = false;
	addInput(new Input<DataBase>(this));
	addOutput<OutputDataDefault<DataBase>>();
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
		std::this_thread::sleep_for(milliseconds(std::min(timeInMs, maxTimeInMs)));
		auto const now = high_resolution_clock::now();
		auto const waitTimeInMs = (duration_cast<milliseconds>(now - lastNow)).count();
		if (waitTimeInMs > timeInMs) {
			if (lastData) outputs[0]->emit(lastData);
			lastNow = high_resolution_clock::now();
		}
	}
}

void Repeater::process(Data data) {
	lastData = data;
	outputs[0]->emit(lastData);
	lastNow = high_resolution_clock::now();
}

}
}
