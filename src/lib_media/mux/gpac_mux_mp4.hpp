#pragma once

#include "lib_modules/core/module.hpp"
#include "../common/libav.hpp"
#include "../common/gpac.hpp"
#include <string>

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
			SegmentAtAny       = 1 << 0, //don't wait for a RAP - automatically set for audio and subtitles
			Browsers           = 1 << 1,
			SmoothStreaming    = 1 << 2,
			SegNumStartsAtZero = 1 << 3,
			SegConstantDur     = 1 << 4, //default is average i.e. segment duration may vary ; with this flag the actual duration may be different from segmentDurationInMs
			ExactInputDur      = 1 << 5, //adds a one sample latency ; default is inferred and smoothen
			NoEditLists        = 1 << 6, 
		};

		GPACMuxMP4(const std::string &baseName, uint64_t segmentDurationInMs = 0, SegmentPolicy segmentPolicy = NoSegment, FragmentPolicy fragmentPolicy = NoFragment, CompatibilityFlag compatFlags = None);
		virtual ~GPACMuxMP4() {}
		void process() override;
		void flush() override;

	protected:
		virtual void declareStreamVideo(const std::shared_ptr<const MetadataPktLibavVideo> &metadata);
		virtual void declareStreamAudio(const std::shared_ptr<const MetadataPktLibavAudio> &metadata);
		virtual void declareStreamSubtitle(const std::shared_ptr<const MetadataPktLibavSubtitle> &metadata);
		virtual void startSegmentPostAction() {}
		uint32_t trackId = 0;
		std::string codec4CC;
		GF_ISOFile *isoInit, *isoCur;

	private:
		void declareStream(const std::shared_ptr<const IMetadata> &metadata);
		void declareInput(const std::shared_ptr<const IMetadata> &metadata);
		void handleInitialTimeOffset();
		void sendOutput();
		std::unique_ptr<gpacpp::IsoSample> fillSample(Data data);
		void startChunk(gpacpp::IsoSample * const sample);
		void addData(gpacpp::IsoSample const * const sample, int64_t lastDataDurationInTs);
		void closeChunk(bool nextSampleIsRAP);
		void processSample(std::unique_ptr<gpacpp::IsoSample> sample, int64_t lastDataDurationInTs);

		CompatibilityFlag compatFlags;
		Data lastData = nullptr; //used with ExactInputDur flag
		int64_t lastInputTimeIn180k = 0, firstDataAbsTimeInMs = 0;
		uint64_t DTS = 0, defaultSampleIncInTs = 0;
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
		uint64_t segmentDurationIn180k, curSegmentDurInTs = 0, curSegmentDeltaInTs = 0, segmentNum = 0, lastSegmentSize = 0;
		bool segmentStartsWithRAP = true;
		std::string segmentName;

		OutputDataDefault<DataRawGPAC> *output;
		union {
			unsigned int resolution[2];
			unsigned int sampleRate;
		};
};

}
}
