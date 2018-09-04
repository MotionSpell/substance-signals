#pragma once

#include <lib_modules/utils/helper.hpp>
#include <thread>
#include <condition_variable>
#include <mutex>

namespace {

struct Passthru : public Modules::ModuleS {
	Passthru() {
		addInput(this);
		addOutput<Modules::OutputDefault>();
	}
	void process(Modules::Data) override {
	}
};

struct InfiniteSource : Modules::ActiveModule {
	InfiniteSource() {
		out = addOutput<Modules::OutputDefault>();
	}
	bool work() {
		out->emit(out->getBuffer(0));
		return true;
	}
	Modules::OutputDefault* out;
};

struct FakeSource : Modules::ActiveModule {
	FakeSource(int maxNumRepetition = 50) : numRepetition(maxNumRepetition) {
		out = addOutput<Modules::OutputDefault>();
	}
	bool work() {
		out->emit(out->getBuffer(0));
		return --numRepetition > 0;
	}
	int numRepetition;
	Modules::OutputDefault* out;
};

struct FakeSink : public Modules::ModuleS {
	FakeSink() {
		addInput(this);
	}
	void process(Modules::Data) override {
	}
};

class DualInput : public Modules::Module {
	public:
		DualInput() {
			input0 = (Modules::Input*)addInput(this);
			input1 = (Modules::Input*)addInput(this);
			out = addOutput<Modules::OutputDefault>();
		}

		void process() {
			if (!done) {
				input0->pop();
				input1->pop();
				out->emit(out->getBuffer(0));
			}

			done = true;

			input0->clear();
			input1->clear();
		}

	private:
		bool done = false;
		Modules::Input* input0;
		Modules::Input* input1;
		Modules::OutputDefault* out;
};

}

class ThreadedDualInput : public Modules::Module {
	public:
		ThreadedDualInput() {
			input0 = (Modules::Input*)addInput(this);
			input1 = (Modules::Input*)addInput(this);
			addOutput<Modules::OutputDefault>();
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
		Modules::Input* input0;
		Modules::Input* input1;
};

