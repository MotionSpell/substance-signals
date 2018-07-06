#include "connection.hpp"
#include "metadata.hpp"
#include "lib_signals/helper.hpp"
#include "log.hpp"

namespace Modules {

Signals::ExecutorSync g_executorSync;

void ConnectOutputToInput(IOutput *prev, IInput *next, IExecutor * const executor) {
	auto prevMetadata = prev->getMetadata();
	auto nextMetadata = next->getMetadata();
	if (prevMetadata && nextMetadata) {
		if (prevMetadata->getStreamType() != next->getMetadata()->getStreamType())
			throw std::runtime_error("Module connection: incompatible types");
		Log::msg(Info, "--------- Connect: metadata OK");
	} else {
		if (prevMetadata && !nextMetadata) {
			Log::msg(Debug, "--------- Connect: metadata doesn't propagate to next (forward)");
		} else if (!prevMetadata && nextMetadata) {
			prev->setMetadata(nextMetadata);
			Log::msg(Info, "--------- Connect: metadata propagates to previous (backward).");
		} else {
			Log::msg(Debug, "--------- Connect: no metadata");
		}
	}

	next->connect();
	prev->getSignal().connect([=](Data data) {
		next->push(data);
		(*executor)(Bind(&IProcessor::process, next));
	});
}

void ConnectModules(IModule *prev, int outputIdx, IModule *next, int inputIdx, IExecutor &executor) {
	auto output = prev->getOutput(outputIdx);
	ConnectOutputToInput(output, next->getInput(inputIdx), &executor);
}

}
