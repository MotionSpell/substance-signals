#pragma once

#include "lib_modules/core/module.hpp"
#include <chrono>
#include <thread>

namespace Modules {
namespace Utils {

/*repeats the last received data every n ms*/
class Repeater : public ModuleS {
public:
	Repeater(int64_t ms);
	virtual ~Repeater();
	void process(Data data) override;
	void flush() override;

private:
	void threadProc();
	std::thread workingThread;
	std::atomic_bool done;
	int64_t periodInMs;
	std::chrono::time_point<std::chrono::high_resolution_clock> lastNow;
	Data lastData;
};

}
}
