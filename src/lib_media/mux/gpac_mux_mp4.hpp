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
		enum ChunkPolicy {
			NoSegment,  //one file
			NoFragment, //several files as in HSS
			OneFragmentPerSegment,
			OneFragmentPerRAP,
			OneFragmentPerFrame,
		};

		GPACMuxMP4(const std::string &baseName, uint64_t chunkDurationInMs = 0, ChunkPolicy chunkPolicy = NoSegment);
		~GPACMuxMP4();
		void process() override;
		void flush() override;

	private:
		void declareStream(Data stream);
		void declareStreamVideo(std::shared_ptr<const MetadataPktLibavVideo> stream);
		void declareStreamAudio(std::shared_ptr<const MetadataPktLibavAudio> stream);
		void sendOutput();
		void addSample(gpacpp::IsoSample &sample, const uint64_t dataDurationInTs);

		GF_ISOFile *m_iso;
		uint32_t m_trackId;
		uint64_t m_DTS = 0, m_prevDTS = 0, m_lastInputTimeIn180k = 0;
		bool isAnnexB = true;

		//fragments
		void setupFragments();
		void startFragment(uint64_t DTS, uint64_t PTS);

		//segments
		void closeSegment(bool isLastSeg);
		ChunkPolicy m_chunkPolicy;
		uint64_t m_chunkDuration, m_curChunkDur = 0, m_curFragmentDur = 0, m_chunkNum = 0, m_lastChunkSize = 0;
		bool m_chunkStartsWithRAP = true;
		std::string m_chunkName;

		OutputDataDefault<DataAVPacket>* output;
		union {
			unsigned int resolution[2];
			unsigned int sampleRate;
		};
};

}
}
