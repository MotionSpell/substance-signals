#pragma once

#include <vector>
#include "lib_modules/utils/helper.hpp"
#include "lib_media/common/file_puller.hpp"

struct IAdaptationControl {
	virtual int getNumAdaptationSets() const = 0;
	virtual int getNumRepresentationsInAdaptationSet(int adaptationSetIdx) const = 0;

	// no more that one stream per adaptation set can be enabled
	// be aware that these calls will block until the command is effectively enqueued
	virtual void enableStream(int asIdx, int repIdx) = 0;
	virtual void disableStream(int asIdx) = 0;
};

struct DashMpd;

namespace Modules {
namespace In {

class MPEG_DASH_Input : public Module, public IAdaptationControl {
	public:
		MPEG_DASH_Input(KHost* host, IFilePullerFactory *filePullerFactory, std::string const &url);
		~MPEG_DASH_Input();
		void process() override;

		// get information to perform bandwidth adaptation
		// disabling all streams will stop the session
		int getNumAdaptationSets() const override;
		int getNumRepresentationsInAdaptationSet(int adaptationSetIdx) const override;
		void enableStream(int asIdx, int repIdx) override;
		void disableStream(int asIdx) override;

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

