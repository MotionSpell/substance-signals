#pragma once

#include "../common/picture.hpp"
#include "lib_modules/core/module.hpp"
#include <mutex>
#include <thread>

struct SDL_Rect;
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
	bool processOneFrame(Data data);
	void createTexture();

	const std::shared_ptr<IClock> m_clock;

	SDL_Window *window = nullptr;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	std::unique_ptr<SDL_Rect> displayrect;
	PictureFormat pictureFormat;

	int64_t m_NumFrames;

	Queue<Data> m_dataQueue; //FIXME: useless now we have input ports
	std::thread workingThread;
};

}
}
