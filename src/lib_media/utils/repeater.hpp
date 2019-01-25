#pragma once

#include "lib_modules/utils/helper.hpp"
#include <chrono>
#include <thread>
#include <atomic>

namespace Modules {
namespace Utils {

/*repeats the last received data every n ms*/
class Repeater : public ModuleS {
	public:
		Repeater(KHost* host, int64_t ms);
		virtual ~Repeater();
		void processOne(Data data) override;
		void flush() override;

	private:
		void threadProc();

		KHost* const m_host;
		KOutput* m_output;
		std::thread workingThread;
		std::atomic_bool done;
		int64_t periodInMs;
		std::chrono::time_point<std::chrono::high_resolution_clock> lastNow;
		Data lastData;
};

}
}
