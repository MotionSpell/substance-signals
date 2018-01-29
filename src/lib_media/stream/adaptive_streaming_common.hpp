#pragma once

#include "lib_modules/core/module.hpp"
#include "lib_gpacpp/gpacpp.hpp"
#include "../common/libav.hpp"
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
	virtual ~IAdaptiveStreamingCommon() noexcept(false) {}
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
		Static,
		Live,
		LiveNonBlocking,
	};
	enum AdaptiveStreamingCommonFlags {
		None = 0,
		DontRenameSegments,
		PresignalNextSegment,
		ForceRealDurations
	};

	AdaptiveStreamingCommon(Type type, uint64_t segDurationInMs, AdaptiveStreamingCommonFlags flags);
	virtual ~AdaptiveStreamingCommon() {}

	void process() override final;
	void flush() override final;

protected:
	std::string getSegmentName(Quality const * const quality, size_t index, u64 segmentNum) const;
	void endOfStream();

	const Type type;
	uint64_t startTimeInMs=-1, segDurationInMs, totalDurationInMs=0, numSeg=0;
	const AdaptiveStreamingCommonFlags flags;
	std::vector<std::unique_ptr<Quality>> qualities;
	OutputDataDefault<DataAVPacket> *outputSegments, *outputManifest;

private:
	void threadProc();
	std::thread workingThread;
};

}
}
