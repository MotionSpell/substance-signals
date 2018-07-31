#pragma once

#include "lib_modules/core/log.hpp"
#include "lib_modules/utils/helper.hpp"

namespace Modules {
namespace Utils {

class PcmComparator : public ModuleS, private LogCap {
	public:
		PcmComparator();
		void process(Data data) override;
		bool compare(Data data1, Data data2) const;
		void pushOriginal(Data data);
		void pushOther(Data data);

	private:
		const float tolerance = 0.0;
		Queue<Data> original, other;
};

}
}
