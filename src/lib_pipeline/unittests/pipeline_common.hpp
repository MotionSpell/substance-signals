#pragma once

#include <thread>

using namespace Modules;

class DualInput : public Module {
	public:
		DualInput(bool threaded) : threaded(threaded) {
			input0 = (Input<DataBase>*)addInput(new Input<DataBase>(this));
			input1 = (Input<DataBase>*)addInput(new Input<DataBase>(this));
			addOutput<OutputDefault>();
			numCalls = 0;
			if (threaded)
				workingThread = std::thread(&DualInput::threadProc, this);
		}

		virtual ~DualInput() {
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
		Input<DataBase>* input0;
		Input<DataBase>* input1;
};
