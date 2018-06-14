#pragma once

using namespace Modules;

class DualInput : public Module {
	public:
		DualInput() {
			input0 = (Input<DataBase>*)addInput(new Input<DataBase>(this));
			input1 = (Input<DataBase>*)addInput(new Input<DataBase>(this));
			addOutput<OutputDefault>();
		}

		void process() {
			if (!done) {
				input0->pop();
				input1->pop();
			}

			done = true;

			input0->clear();
			input1->clear();
		}

	private:
		bool done = false;
		Input<DataBase>* input0;
		Input<DataBase>* input1;
};

#include <thread>
#include <condition_variable>
#include <mutex>

class ThreadedDualInput : public Module {
	public:
		ThreadedDualInput() {
			input0 = (Input<DataBase>*)addInput(new Input<DataBase>(this));
			input1 = (Input<DataBase>*)addInput(new Input<DataBase>(this));
			addOutput<OutputDefault>();
			numCalls = 0;
			workingThread = std::thread(&ThreadedDualInput::threadProc, this);
		}

		virtual ~ThreadedDualInput() {
			if (workingThread.joinable()) {
				for(auto& input : inputs)
					input->push(nullptr);
				workingThread.join();
			}
		}

		void process() {
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
				input0->pop();
				input1->pop();
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
		bool done = false;
		std::thread workingThread;
		std::mutex m_protectDone;
		std::condition_variable flushed;
		Input<DataBase>* input0;
		Input<DataBase>* input1;
};
