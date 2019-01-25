#pragma once

#include "lib_utils/queue.hpp"
#include "lib_modules/utils/helper.hpp"

namespace Modules {
namespace Utils {

class Recorder : public ModuleS {
	public:
		Recorder(KHost* host);
		void processOne(Data data) override;
		void flush() override;

		Data pop();

	private:
		KHost* const m_host;
		Queue<Data> record;
};

}
}
