// holds the chain: [dash downloader] => ( [mp4demuxer] => [restamper] )*
#include "lib_utils/log_sink.hpp"
#include "lib_modules/utils/factory.hpp"
#include "hls_demux.hpp"
#include "lib_media/in/mpeg_dash_input.hpp"
#include "lib_media/out/null.hpp"
#include "lib_media/transform/restamp.hpp"
#include <sstream>

using namespace std;
using namespace Modules;
using namespace In;
using namespace Transform;

unique_ptr<Modules::In::IFilePuller> createHttpSource();

namespace {

class HlsDemuxer : public ActiveModule {
	public:
		HlsDemuxer(KHost* host, HlsDemuxConfig* cfg)
			: m_host(host),
			  m_playlistUrl(cfg->url) {
			m_puller = cfg->filePuller;
			if(!m_puller) {
				m_internalPuller = createHttpSource();
				m_puller = m_internalPuller.get();
			}
		}

		virtual bool work() override {
			auto main = downloadPlaylist(m_playlistUrl);
			if(main.empty()) {
				m_host->log(Error, "No main playlist");
				return false;
			}

			string subUrl = main[0];

			for(auto chunkUrl : downloadPlaylist(subUrl))
				m_puller->get(chunkUrl.c_str());

			return false;
		}

		vector<string> downloadPlaylist(string url) {
			auto contents = m_puller->get(url.c_str());
			vector<string> r;
			string line;
			stringstream ss(string(contents.begin(), contents.end()));
			while(getline(ss, line)) {
				if(line.empty() || line[0] == '#')
					continue;
				r.push_back(line);
			}
			return r;
		}

	private:
		KHost* const m_host;
		string const m_playlistUrl;
		IFilePuller* m_puller;
		unique_ptr<IFilePuller> m_internalPuller;
};

Modules::IModule* createObject(KHost* host, void* va) {
	auto config = (HlsDemuxConfig*)va;
	enforce(host, "HlsDemuxer: host can't be NULL");
	enforce(config, "HlsDemuxer: config can't be NULL");
	return Modules::create<HlsDemuxer>(host, config).release();
}

auto const registered = Factory::registerModule("HlsDemuxer", &createObject);
}
