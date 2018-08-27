#include "lib_utils/tools.hpp"
#include "recorder.hpp"

namespace Modules {
namespace Utils {

Recorder::Recorder(IModuleHost* host)
	: m_host(host) {
	createInput(this);
}

void Recorder::flush() {
	record.clear();
}

void Recorder::process(Data data) {
	if (data) {
		m_host->log(Debug, format("Data[%s] recorded at media time %s", data, data->getMediaTime()).c_str());
	}
	record.push(data);
}

Data Recorder::pop() {
	return record.pop();
}

}
}
