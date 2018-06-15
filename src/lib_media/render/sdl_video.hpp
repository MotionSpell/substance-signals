#pragma once

#include "../common/picture.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_utils/default_clock.hpp"
#include <thread>

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

namespace Modules {
namespace Render {

class SDLVideo : public ModuleS {
	public:
		SDLVideo(const std::shared_ptr<IClock> clock = g_DefaultClock);
		~SDLVideo();
		void process(Data data) override;

	private:
		void doRender();
		void displayFrame(Data data);
		void present();
		bool processEvents();
		void createTexture();

		const std::shared_ptr<IClock> m_clock;

		SDL_Window *window = nullptr;
		SDL_Renderer *renderer;
		SDL_Texture *texture;
		Resolution displaySize;
		PictureFormat pictureFormat;
		bool respectTimestamps;

		Queue<Data> m_dataQueue; //FIXME: useless now we have input ports
		std::thread workingThread;
};

}
}
