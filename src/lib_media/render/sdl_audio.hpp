#pragma once

#include "lib_modules/utils/helper.hpp"
#include "lib_utils/default_clock.hpp"
#include "../common/pcm.hpp"
#include "lib_utils/fifo.hpp"
#include <memory>
#include <mutex>

namespace Modules {
namespace Render {

struct Span {
	uint8_t* ptr;
	size_t len;
};

class SDLAudio : public ModuleS {
	public:
		SDLAudio(const std::shared_ptr<IClock> clock = g_DefaultClock);
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
		std::mutex m_Mutex;
		Fifo m_Fifo;
		int64_t fifoTimeIn180k, m_LatencyIn180k;
};

}
}
