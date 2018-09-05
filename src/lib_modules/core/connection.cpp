#include "connection.hpp"
#include "metadata.hpp"
#include "lib_signals/helper.hpp"
#include "log.hpp"

namespace Modules {

Signals::ExecutorSync g_executorSync;

void CheckMetadataCompatibility(IOutput *prev, IInput *next) {
	auto prevMetadata = prev->getMetadata();
	auto nextMetadata = next->getMetadata();
	if (prevMetadata && nextMetadata) {
		if (prevMetadata->type != next->getMetadata()->type)
			throw std::runtime_error("Module connection: incompatible types");
		g_Log->log(Info, "--------- Connect: metadata OK");
	} else {
		if (prevMetadata && !nextMetadata) {
			g_Log->log(Debug, "--------- Connect: metadata doesn't propagate to next (forward)");
		} else if (!prevMetadata && nextMetadata) {
			prev->setMetadata(nextMetadata);
			g_Log->log(Info, "--------- Connect: metadata propagates to previous (backward).");
		} else {
			g_Log->log(Debug, "--------- Connect: no metadata");
		}
	}
}

void ConnectOutputToInput(IOutput *prev, IInput *next, IExecutor * const executor) {
	CheckMetadataCompatibility(prev, next);

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
