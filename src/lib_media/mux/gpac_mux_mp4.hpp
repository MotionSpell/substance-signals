#pragma once

#include "lib_modules/core/module.hpp"
#include "../common/libav.hpp"
#include <string>

typedef struct __tag_isom GF_ISOFile;
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
			IndependentSegment, //starts with moov, no initialization segment
			FragmentedSegment, //starts with moof, initialization segment
		};
		enum FragmentPolicy {
			NoFragment,
			OneFragmentPerSegment,
			OneFragmentPerRAP,
			OneFragmentPerFrame,
		};

		GPACMuxMP4(const std::string &baseName, uint64_t segmentDurationInMs = 0, SegmentPolicy segmentPolicy = NoSegment, FragmentPolicy fragmentPolicy = NoFragment);
		~GPACMuxMP4();
		void process() override;
		void flush() override;

	private:
		void declareStream(Data stream);
		void declareStreamVideo(std::shared_ptr<const MetadataPktLibavVideo> stream, bool declareInput);
		void declareStreamAudio(std::shared_ptr<const MetadataPktLibavAudio> stream, bool declareInput);
		void sendOutput();
		gpacpp::IsoSample fillSample(Data data);
		void addSample(gpacpp::IsoSample &sample, const uint64_t dataDurationInTs);

		GF_ISOFile *isoInit, *isoCur;
		uint32_t trackId;
		uint64_t DTS = 0, prevDTS = 0, lastInputTimeIn180k = 0;
		bool isAnnexB = true;

		//fragments
		void setupFragments();
		void startFragment(uint64_t DTS, uint64_t PTS);
		void closeFragment();
		FragmentPolicy fragmentPolicy;
		uint64_t curFragmentDur = 0;

		//segments
		void startSegment();
		void closeSegment(bool isLastSeg);
		SegmentPolicy segmentPolicy;
		uint64_t segmentDuration, curSegmentDur = 0, segmentNum = 0, lastSegmentSize = 0;
		bool segmentStartsWithRAP = true;
		std::string segmentName;

		OutputDataDefault<DataAVPacket>* output;
		union {
			unsigned int resolution[2];
			unsigned int sampleRate;
		};
};

}
}
