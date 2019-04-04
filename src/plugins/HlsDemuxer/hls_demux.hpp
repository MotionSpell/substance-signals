#pragma once

#include <string>
#include "lib_media/common/file_puller.hpp"

struct HlsDemuxConfig {
	std::string url;
	Modules::In::IFilePuller* filePuller = nullptr; // if null, use internal HTTP puller.
};

