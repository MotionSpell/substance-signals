#pragma once

#include "lib_modules/utils/helper.hpp"

namespace Modules {
namespace Transform {

class Restamp : public ModuleS, private LogCap {
	public:
		enum Mode {
			Passthru,            /*offset only*/
			Reset,               /*set the first received timestamp to 0 - aside from the offsetIn180k param*/
			ClockSystem,         /*the system clock: starts at 0 on first packet*/
			IgnoreFirstAndReset, /*with RTP input, libavdemux put a wrong first timestamp*/
		};

		/*offset will be added to the current time*/
		Restamp(Mode mode, int64_t offsetIn180k = 0);
		~Restamp();
		int64_t restamp(int64_t time);
		void process(Data data) override;

	private:
		int64_t offset;
		Mode mode;
		bool isInitTime = false;
};

}
}
