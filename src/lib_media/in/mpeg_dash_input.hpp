#pragma once

#include <vector>
#include "lib_modules/utils/helper.hpp"
#include "lib_media/common/file_puller.hpp"

struct DashMpd;

namespace Modules {
namespace In {

class MPEG_DASH_Input : public Module {
	public:
		MPEG_DASH_Input(KHost* host, IFilePullerFactory *filePullerFactory, std::string const &url);
		~MPEG_DASH_Input();
		void process() override;

	private:
		KHost* const m_host;

		std::vector<std::unique_ptr<IFilePuller>> m_sources;

		std::unique_ptr<DashMpd> mpd;
		std::string m_mpdDirname;

		struct Stream;
		std::vector<std::unique_ptr<Stream>> m_streams;
		void processStream(Stream* stream);
};

}
}

