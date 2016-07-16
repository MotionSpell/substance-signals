#pragma once

#include "lib_modules/core/module.hpp"
#include "lib_gpacpp/gpacpp.hpp"
#include <memory>

namespace Modules {
namespace Stream {

struct Quality {
	Quality() : meta(nullptr), avg_bitrate_in_bps(0) {}
	virtual ~Quality() {}
	std::shared_ptr<const MetadataFile> meta;
	double avg_bitrate_in_bps;
};

struct IAdaptiveStreamingCommon {
	/*created each quality private data*/
	virtual std::unique_ptr<Quality> createQuality() const = 0;
	/*called each time segments are ready*/
	virtual void generateManifest() = 0;
	/*last manifest to be written: usually the VoD one*/
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
	uint64_t startTimeInMs, segDurationInMs, totalDurationInMs;
	std::vector<std::unique_ptr<Quality>> qualities;

private:
	void threadProc();
	void endOfStream();
	int numDataQueueNotify = 0;
	std::thread workingThread;
};

}
}