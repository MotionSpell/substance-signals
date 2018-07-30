#include "lib_utils/tools.hpp"
#include "recorder.hpp"

namespace Modules {
namespace Utils {

Recorder::Recorder() {
	createInput(this);
}

void Recorder::flush() {
	record.clear();
}

void Recorder::process(Data data) {
	if (data) {
		log(Debug, "Data[%s] recorded at media time %s", data, data->getMediaTime());
	}
	record.push(data);
}

Data Recorder::pop() {
	return record.pop();
}

}
}
