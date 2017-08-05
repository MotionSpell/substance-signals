#pragma once

#include "lib_modules/core/module.hpp"
#include "../common/libav.hpp"
#include "../common/gpac.hpp"
#include <string>

//#define DURATION_KEEP_LAST_DATA

typedef struct __tag_isom GF_ISOFile;
typedef struct __tag_bitstream GF_BitStream;
namespace gpacpp {
class IsoSample;
}

namespace Modules {
namespace Mux {

class GPACMuxMP4 : public ModuleDynI {
	public:
		enum SegmentPolicy {
			NoSegment,
			SingleSegment,
			IndependentSegment, //starts with moov, no init segment, no 'styp', moof with contiguous seq_nb
			FragmentedSegment,  //starts with moof, initialization segment
		};
		enum FragmentPolicy {
			NoFragment,
			OneFragmentPerSegment,
			OneFragmentPerRAP,
			OneFragmentPerFrame,
		};

		enum CompatibilityFlag {
			None               = 0,
			SegmentAtAny       = 1, //don't wait for a RAP
			Browsers           = 1 << 1,
			SmoothStreaming    = 1 << 2,
			SegNumStartsAtZero = 1 << 3,
			SegConstantDur     = 1 << 4 //default is average i.e. segment duration may vary ; with this flag the actual duration may be different from segmentDurationInMs
		};

		GPACMuxMP4(const std::string &baseName, uint64_t segmentDurationInMs = 0, SegmentPolicy segmentPolicy = NoSegment, FragmentPolicy fragmentPolicy = NoFragment, CompatibilityFlag compatFlags = None);
		virtual ~GPACMuxMP4() {}
		void process() override;
		void flush() override;

	protected:
		virtual void declareStreamVideo(std::shared_ptr<const MetadataPktLibavVideo> stream);
		virtual void declareStreamAudio(std::shared_ptr<const MetadataPktLibavAudio> stream);
		virtual void declareStreamSubtitle(std::shared_ptr<const MetadataPktLibavSubtitle> metadata);
		virtual void startSegmentPostAction() {}
		uint32_t trackId;
		std::string codec4CC;
		GF_ISOFile *isoInit, *isoCur;

	private:
		void declareStream(Data stream);
		void declareInput(std::shared_ptr<const MetadataPktLibav> metadata);
		void sendOutput();
		std::unique_ptr<gpacpp::IsoSample> fillSample(Data data);
		void addSample(gpacpp::IsoSample &sample, const uint64_t dataDurationInTs);

		CompatibilityFlag compatFlags;
		uint64_t DTS = 0, prevDTS = 0, lastInputTimeIn180k = 0, defaultSampleIncInTs = 0, firstDataAbsTimeInMs = 0;
		bool isAnnexB = true;

		//fragments
		void setupFragments();
		void startFragment(uint64_t DTS, uint64_t PTS);
		void closeFragment();
		const FragmentPolicy fragmentPolicy;
		uint64_t curFragmentDurInTs = 0;
		//SmoothStreaming compat only, for fragments:
		uint64_t nextFragmentNum/*used with IndependentSegment*/ = 1; 

		//segments
		void startSegment();
		void closeSegment(bool isLastSeg);
		const SegmentPolicy segmentPolicy;
		uint64_t segmentDurationIn180k, curSegmentDurInTs = 0, deltaInTs = 0, segmentNum = 0, lastSegmentSize = 0;
		bool segmentStartsWithRAP = true;
		std::string segmentName;

		OutputDataDefault<DataRawGPAC>* output;
		union {
			unsigned int resolution[2];
			unsigned int sampleRate;
		};

#ifdef DURATION_KEEP_LAST_DATA
		Data lastData = nullptr;
#endif
};

class GPACMuxMP4MSS : public GPACMuxMP4 {
public:
	GPACMuxMP4MSS(const std::string &baseName, uint64_t segmentDurationInMs, const std::string &audioLang = "", const std::string &audioName = "");

private:
	void declareStreamVideo(std::shared_ptr<const MetadataPktLibavVideo> stream) final;
	void declareStreamAudio(std::shared_ptr<const MetadataPktLibavAudio> metadata) final;
	void declareStreamSubtitle(std::shared_ptr<const MetadataPktLibavSubtitle> metadata) final;
	void startSegmentPostAction() final;

	std::string writeISMLManifest(std::string codec4CC, std::string codecPrivate, int64_t bitrate, int width, int height, uint32_t sampleRate, uint32_t channels, uint16_t bitsPerSample);
	std::string ISMLManifest;
	const std::string audioLang, audioName;
};

}
}
