#pragma once

#include "lib_modules/core/module.hpp"
#include "lib_gpacpp/gpacpp.hpp" //Romain

namespace Modules {
namespace Stream {

struct IAdaptiveStreamingCommon {
	virtual void generateManifest() = 0;
	virtual void finalizeManifest() = 0;
};

class AdaptiveStreamingCommon : public IAdaptiveStreamingCommon, public ModuleDynI {
public:
	enum Type {
		Live,
		Static
	};

	AdaptiveStreamingCommon(Type type, uint64_t segDurationInMs);
	virtual ~AdaptiveStreamingCommon();

	void process() override final;
	void flush() override final;

protected:
	Type type;
	uint64_t startTimeInMs, segDurationInMs, totalDurationInMs; //Romain: initializers

	struct Quality {
		Quality() : meta(nullptr), bitrate_in_bps(0), rep(nullptr) {}
		std::shared_ptr<const MetadataFile> meta;
		double bitrate_in_bps;
		GF_MPD_Representation *rep;
	};
	std::vector<Quality> qualities;

private:
	void threadProc();
	void endOfStream();
	int numDataQueueNotify = 0;
	std::thread workingThread;
};

}
}