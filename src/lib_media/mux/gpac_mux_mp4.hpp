#pragma once

#include "mux_mp4_config.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_modules/utils/helper_dyn.hpp"
#include "../common/libav.hpp"

typedef struct __tag_isom GF_ISOFile;
namespace gpacpp {
class IsoSample;
}
namespace Modules {

class DataRawGPAC;

namespace Mux {

class GPACMuxMP4 : public ModuleDynI {
	public:
		GPACMuxMP4(IModuleHost* host, Mp4MuxConfig const& config);
		virtual ~GPACMuxMP4();
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
		IModuleHost * const m_host;
		IUtcStartTimeQuery* const m_utcStartTime;

		bool processInit(Data &data);
		void declareStream(const Metadata &metadata);
		void handleInitialTimeOffset();
		void sendOutput(bool EOS);
		std::unique_ptr<gpacpp::IsoSample> fillSample(Data data);
		void startChunk(gpacpp::IsoSample * const sample);
		void addData(gpacpp::IsoSample const * const sample, int64_t lastDataDurationInTs);
		void closeChunk(bool nextSampleIsRAP);
		void processSample(std::unique_ptr<gpacpp::IsoSample> sample, int64_t lastDataDurationInTs);
		void updateSegmentName();

		CompatibilityFlag compatFlags;
		Data lastData = nullptr; //used with ExactInputDur flag
		int64_t DTS = 0, initDTSIn180k = 0, firstDataAbsTimeInMs = 0;
		uint64_t defaultSampleIncInTs = 0;
		uint32_t mediaTs = 0;
		bool isAnnexB = true;

		//fragments
		void setupFragments();
		void startFragment(uint64_t DTS, uint64_t PTS);
		void closeFragment();
		const FragmentPolicy fragmentPolicy;
		int64_t curFragmentDurInTs = 0;
		//SmoothStreaming compat only, for fragments:
		uint64_t nextFragmentNum/*used with IndependentSegment*/ = 1;

		//segments
		void startSegment();
		void closeSegment(bool isLastSeg);
		const SegmentPolicy segmentPolicy;
		Fraction segmentDuration {};
		uint64_t curSegmentDurInTs = 0, curSegmentDeltaInTs = 0, segmentNum = 0, lastSegmentSize = 0;
		bool segmentStartsWithRAP = true;
		std::string segmentName;

		OutputDataDefault<DataRawGPAC> *output;
		union {
			Resolution resolution;
			int sampleRate;
		};
};

}
}
