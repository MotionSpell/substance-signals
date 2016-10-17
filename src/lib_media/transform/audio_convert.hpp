#pragma once

#include "lib_modules/core/module.hpp"
#include "../common/pcm.hpp"


namespace ffpp {
class SwResampler;
class Frame;
}

namespace Modules {
namespace Transform {

class AudioConvert : public ModuleS {
	public:
		/*dstFrameSize is the number of output sample - '-1' is same as input*/
		AudioConvert(const PcmFormat &dstFormat, int64_t dstNumSamples = -1);
		AudioConvert(const PcmFormat &srcFormat, const PcmFormat &dstFormat, int64_t dstNumSamples = -1);
		~AudioConvert();
		void process(Data data) override;
		void flush() override;

	private:
		void configure(const PcmFormat &srcFormat);
		void reconfigure(const PcmFormat &srcFormat);

		PcmFormat srcPcmFormat;
		PcmFormat const dstPcmFormat;
		int64_t dstNumSamples, curDstNumSamples = 0;
		std::shared_ptr<DataPcm> curOut;
		std::unique_ptr<ffpp::SwResampler> m_Swr;
		uint64_t accumulatedTimeInDstSR = 0;
		OutputPcm* output;
		bool autoConfigure;
};

}
}
