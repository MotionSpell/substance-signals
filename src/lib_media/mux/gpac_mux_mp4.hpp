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
			None			= 0,
			DashJs			= 1,
			SmoothStreaming	= 1 << 1,
		};

		GPACMuxMP4(const std::string &baseName, uint64_t segmentDurationInMs = 0, SegmentPolicy segmentPolicy = NoSegment, FragmentPolicy fragmentPolicy = NoFragment, CompatibilityFlag compatFlags = None);
		~GPACMuxMP4();
		void process() override;
		void flush() override;

	private:
		void declareStream(Data stream);
		void declareStreamVideo(std::shared_ptr<const MetadataPktLibavVideo> stream, bool declareInput);
		void declareStreamAudio(std::shared_ptr<const MetadataPktLibavAudio> stream, bool declareInput);
		void sendOutput();
		std::unique_ptr<gpacpp::IsoSample> fillSample(Data data);
		void addSample(gpacpp::IsoSample &sample, const uint64_t dataDurationInTs);

		CompatibilityFlag compatFlags;
		GF_ISOFile *isoInit, *isoCur;
		uint32_t trackId;
		uint64_t DTS = 0, prevDTS = 0, lastInputTimeIn180k = 0;
		bool isAnnexB = true;

		//fragments
		void setupFragments();
		void startFragment(uint64_t DTS, uint64_t PTS);
		void closeFragment();
		FragmentPolicy fragmentPolicy;
		uint64_t curFragmentDurInTs = 0;
		uint64_t nextFragmentNum = 1; /*used with IndependentSegment and SmoothStreaming compat only*/

		//segments
		void startSegment();
		void closeSegment(bool isLastSeg);
		SegmentPolicy segmentPolicy;
		uint64_t segmentDurationIn180k, curSegmentDurInTs = 0, segmentNum = 0, lastSegmentSize = 0;
		bool segmentStartsWithRAP = true;
		std::string segmentName;

		//smooth streaming specific
		std::string writeISMLManifest();

		OutputDataDefault<DataAVPacket>* output;
		union {
			unsigned int resolution[2];
			unsigned int sampleRate;
		};
};

inline GPACMuxMP4::CompatibilityFlag operator | (GPACMuxMP4::CompatibilityFlag a, GPACMuxMP4::CompatibilityFlag b) {
	return static_cast<GPACMuxMP4::CompatibilityFlag>(static_cast<int>(a) | static_cast<int>(b));
}

}
}
