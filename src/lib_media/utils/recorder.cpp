#include "lib_utils/tools.hpp"
#include "lib_utils/log_sink.hpp" // Warning
#include "lib_utils/format.hpp"
#include "recorder.hpp"

namespace Modules {
namespace Utils {

Recorder::Recorder(KHost* host)
	: m_host(host) {
}

void Recorder::flush() {
	record.clear();
}

void Recorder::processOne(Data data) {
	if (data) {
		m_host->log(Debug, format("Data[%s] recorded at media time %s", data.get(), data->getMediaTime()).c_str());
	}
	record.push(data);
}

Data Recorder::pop() {
	return record.pop();
}

}
}
