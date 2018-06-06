#pragma once

#include "lib_modules/utils/helper.hpp"
#include "../common/pcm.hpp"
#include "lib_utils/fifo.hpp"
#include <memory>
#include <mutex>
#include <memory.h>

struct SDL_Rect;
struct SDL_Renderer;
struct SDL_Texture;

namespace Modules {
namespace Render {

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
		void fillAudio(uint8_t *stream, int len);

		uint64_t fifoSamplesToRead() const;
		void fifoConsumeSamples(size_t n);
		void writeSamples(uint8_t*& dst, uint8_t const* src, size_t n);
		void silenceSamples(uint8_t*& dst, size_t n);

		const std::shared_ptr<IClock> m_clock;
		PcmFormat m_outputFormat;
		PcmFormat m_inputFormat;
		std::unique_ptr<ModuleS> m_converter;
		std::mutex m_Mutex;
		Fifo m_Fifo;
		int64_t fifoTimeIn180k, m_LatencyIn180k;
};

}
}
