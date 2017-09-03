#include "lib_utils/tools.hpp"
#include "recorder.hpp"

namespace Modules {
namespace Utils {

Recorder::Recorder() {
	addInput(new Input<DataBase>(this));
}

void Recorder::flush() {
	record.clear();
}

void Recorder::process(Data data) {
	if (data) {
		log(Debug, "Recorded at media time %s (clock time: %s)", data, data->getMediaTime(), data->getClockTime());
	}
	record.push(data);
}

Data Recorder::pop() {
	return record.pop();
}

}
}
