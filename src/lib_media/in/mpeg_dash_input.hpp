#pragma once

#include <vector>
#include "lib_modules/utils/helper.hpp"
#include "lib_media/common/file_puller.hpp"

struct DashMpd;

namespace Modules {
namespace In {

class MPEG_DASH_Input : public ActiveModule {
	public:
		MPEG_DASH_Input(KHost* host, IFilePuller* filePuller, std::string const &url);
		~MPEG_DASH_Input();
		bool work() override;

	private:
		KHost* const m_host;

		IFilePuller* const m_source;
		bool wakeUp();

		struct Stream;
		std::vector<std::unique_ptr<Stream>> m_streams;

		std::unique_ptr<DashMpd> mpd;
		std::string m_mpdDirname;
		bool m_initializationChunkSent = false;
};

}
}

