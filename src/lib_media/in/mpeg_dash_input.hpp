#pragma once

#include <vector>
#include "lib_modules/utils/helper.hpp"

struct DashMpd;

namespace Modules {
namespace In {

struct IFilePuller {
	virtual ~IFilePuller() = default;
	virtual std::vector<uint8_t> get(std::string url) = 0;
};

class MPEG_DASH_Input : public ActiveModule {
	public:
		MPEG_DASH_Input(std::unique_ptr<IFilePuller> filePuller, std::string const &url);
		~MPEG_DASH_Input();
		bool work() override;

	private:
		std::unique_ptr<IFilePuller> const m_source;
		bool wakeUp();

		struct Stream;
		std::vector<std::unique_ptr<Stream>> m_streams;

		std::unique_ptr<DashMpd> mpd;
		std::string m_mpdDirname;
		bool m_initializationChunkSent = false;
};

}
}

