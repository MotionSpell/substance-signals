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
		log(Debug, "Data[%s] recorded at media time %s (creation time: %s)", data, data->getMediaTime(), data->getCreationTime());
	}
	record.push(data);
}

Data Recorder::pop() {
	return record.pop();
}

}
}
