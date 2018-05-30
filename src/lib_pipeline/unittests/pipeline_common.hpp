#pragma once

#include <mutex>
#include <thread>

using namespace Modules;

class DualInput : public Module {
	public:
		DualInput(bool threaded) : threaded(threaded) {
			input0 = (Input<DataBase>*)addInput(new Input<DataBase>(this));
			input1 = (Input<DataBase>*)addInput(new Input<DataBase>(this));
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
				auto i1 = input0->pop();
				auto i2 = input1->pop();
				done = true;
			}

			input0->clear();
			input1->clear();
		}

		static uint64_t numCalls;

	private:
		bool done = false, threaded;
		std::thread workingThread;
		std::mutex numCallsMutex;
		Input<DataBase>* input0;
		Input<DataBase>* input1;
};
