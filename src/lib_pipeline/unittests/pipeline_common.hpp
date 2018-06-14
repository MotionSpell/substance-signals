#pragma once

#include <thread>
#include <condition_variable>
#include <mutex>

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
				for(auto& input : inputs)
					input->push(nullptr);
				workingThread.join();
			}
		}

		void process() {
			if (!threaded) {
				threadProc();
			}
		}

		void flush() {
			std::unique_lock<std::mutex> lock(m_protectDone);
			flushed.wait(lock, [this]() {
				return done;
			});
		}

		void threadProc() {
			numCalls++;

			if (!done) {
				auto i1 = input0->pop();
				auto i2 = input1->pop();
			}

			{
				std::unique_lock<std::mutex> lock(m_protectDone);
				done = true;
				flushed.notify_one();
			}

			input0->clear();
			input1->clear();
		}

		static uint64_t numCalls;

	private:
		bool done = false, threaded;
		std::thread workingThread;
		std::mutex m_protectDone;
		std::condition_variable flushed;
		Input<DataBase>* input0;
		Input<DataBase>* input1;
};
