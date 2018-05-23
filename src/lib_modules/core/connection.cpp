#include "connection.hpp"
#include "metadata.hpp"
#include "lib_signals/utils/helper.hpp"

namespace Modules {

Signals::ExecutorSync<void()> g_executorSync;

size_t ConnectOutputToInput(IOutput *prev, IInput *next, IProcessExecutor * const executor) {
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
	return prev->getSignal().connect([=](Data data) {
		next->push(data);
		(*executor)(Bind(&IProcessor::process, next));
	});
}

size_t ConnectModules(IModule *prev, size_t outputIdx, IModule *next, size_t inputIdx, IProcessExecutor &executor) {
	auto output = prev->getOutput(outputIdx);
	return ConnectOutputToInput(output, next->getInput(inputIdx), &executor);
}

}
