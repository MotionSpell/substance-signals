#pragma once

#include "lib_modules/core/module.hpp"
#include "lib_gpacpp/gpacpp.hpp"
#include "../common/libav.hpp"
#include <memory>
#include <string>

namespace Modules {
namespace Stream {

struct Quality {
	Quality() : meta(nullptr), avg_bitrate_in_bps(0) {}
	virtual ~Quality() {}
	std::shared_ptr<const MetadataFile> meta;
	uint64_t avg_bitrate_in_bps;
	std::string prefix; //typically a subdir, ending with a folder separator
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
		SegmentsNotOwned     = 1 << 0, //don't touch files
		PresignalNextSegment = 1 << 1,
		ForceRealDurations   = 1 << 2
	};

	AdaptiveStreamingCommon(Type type, uint64_t segDurationInMs, const std::string &manifestDir, AdaptiveStreamingCommonFlags flags);
	virtual ~AdaptiveStreamingCommon() {}

	void process() override final;
	void flush() override final;

protected:
	std::string getInitName(Quality const * const quality, size_t index) const;
	std::string getSegmentName(Quality const * const quality, size_t index, const std::string &segmentNumSymbol) const;
	uint64_t getCurSegNum() const;
	void endOfStream();

	const Type type;
	uint64_t startTimeInMs=-1, segDurationInMs, totalDurationInMs=0;
	const std::string manifestDir;
	const AdaptiveStreamingCommonFlags flags;
	std::vector<std::unique_ptr<Quality>> qualities;
	OutputDataDefault<DataAVPacket> *outputSegments, *outputManifest;

private:
	virtual void processInitSegment(Quality const * const quality, size_t index) = 0;
	void ensurePrefix(size_t index);
	std::string getPrefix(Quality const * const quality, size_t index) const;
	void threadProc();
	std::thread workingThread;
};

}
}
