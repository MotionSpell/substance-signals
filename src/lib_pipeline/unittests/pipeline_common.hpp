#pragma once

#include <mutex>
#include <thread>

using namespace Modules;

class DualInput : public Module {
	public:
		DualInput(bool threaded) : threaded(threaded) {
			addInput(new Input<DataBase>(this));
			addInput(new Input<DataBase>(this));
			addOutput<OutputDefault>();
			numCallsMutex.lock();
			numCalls = 0;
			if (threaded)
				workingThread = std::thread(&DualInput::threadProc, this);
		}

		virtual ~DualInput() {
			numCallsMutex.unlock();
			if (workingThread.joinable()) {
				for (size_t i = 0; i < inputs.size(); ++i) {
					inputs[i]->push(nullptr);
				}
				workingThread.join();
			}
		}

		void process() {
			if (!threaded) {
				threadProc();
			}
		}

		void threadProc() {
			numCalls++;

			if (!done) {
				auto i1 = getInput(0)->pop();
				auto i2 = getInput(1)->pop();
				done = true;
			}

			getInput(0)->clear();
			getInput(1)->clear();
		}

		static uint64_t numCalls;

	private:
		bool done = false, threaded;
		std::thread workingThread;
		std::mutex numCallsMutex;
};
