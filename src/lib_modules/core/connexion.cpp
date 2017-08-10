#include "connexion.hpp"
#include "metadata.hpp"
#include "lib_signals/utils/helper.hpp"

namespace Modules {

Signals::ExecutorSync<void()> g_executorSync;

size_t ConnectOutputToInput(IOutput *prev, IInput *next, IProcessExecutor * const executor) {
	auto prevMetadata = safe_cast<const IMetadataCap>(prev)->getMetadata();
	auto nextMetadata = next->getMetadata();
	if (prevMetadata && nextMetadata) {
		if (prevMetadata->getStreamType() != next->getMetadata()->getStreamType())
			throw std::runtime_error("Module connection: incompatible types");
		Log::msg(Info, "--------- Connect: metadata OK");
	} else {
		if (prevMetadata && !nextMetadata) {
			Log::msg(Info, "--------- Connect: metadata is not the same as next");
		} else if (!prevMetadata && nextMetadata) {
			safe_cast<IMetadataCap>(prev)->setMetadata(nextMetadata);
			Log::msg(Info, "--------- Connect: metadata propagate to previous (backward).");
		} else {
			Log::msg(Debug, "--------- Connect: no metadata");
		}
	}

	next->connect();
	return prev->getSignal().connect([=](Data data) {
		next->push(data);
		(*executor)(MEMBER_FUNCTOR_PROCESS(next));
	});
}

}
