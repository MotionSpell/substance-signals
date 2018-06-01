#pragma once

#include <string>
#include "lib_modules/utils/helper.hpp"

struct DashMpd;

namespace Modules {
namespace In {

struct IFilePuller {
	virtual std::string get(std::string url) = 0;
};

class MPEG_DASH_Input : public Module {
	public:
		MPEG_DASH_Input(std::unique_ptr<IFilePuller> filePuller, std::string const &url);
		~MPEG_DASH_Input();
		void process() override;

	private:
		std::unique_ptr<IFilePuller> const m_source;
		bool wakeUp();

		std::unique_ptr<DashMpd> mpd;
		std::string m_mpdDirname;
};

}
}

