#pragma once

#include "lib_modules/utils/helper.hpp"
#include "../common/pcm.hpp"
#include "lib_utils/fifo.hpp"
#include <memory>
#include <mutex>

namespace Modules {
namespace Render {

class SDLAudio : public ModuleS {
	public:
		SDLAudio(std::shared_ptr<IClock> clock = nullptr);
		~SDLAudio();
		void process(Data data) override;
		void flush() override;

	private:
		bool reconfigure(PcmFormat pcmFormat);
		void push(Data data);
		static void staticFillAudio(void *udata, uint8_t *stream, int len);
		void fillAudio(Span buffer);

		uint64_t fifoSamplesToRead() const;
		void fifoConsumeSamples(size_t n);
		void writeSamples(Span& dst, uint8_t const* src, int n);
		void silenceSamples(Span& dst, int n);

		const std::shared_ptr<IClock> m_clock;
		PcmFormat m_outputFormat;
		PcmFormat m_inputFormat;
		std::unique_ptr<IModule> m_converter;
		int64_t m_LatencyIn180k;

		// shared state between:
		// - the producer thread ('push')
		// - the SDL thread ('fillAudio')
		std::mutex m_protectFifo;
		Fifo m_fifo;
		int64_t m_fifoTime;
};

}
}
