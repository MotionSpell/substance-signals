// holds the chain: [dash downloader] => ( [mp4demuxer] => [restamper] )*
#include "lib_utils/log_sink.hpp"
#include "lib_modules/utils/factory.hpp"
#include "hls_demux.hpp"
#include "lib_modules/utils/helper.hpp" // ActiveModule
#include <sstream>
#include <memory>
#include <string.h> // memcpy

using namespace std;
using namespace Modules;
using namespace In;

unique_ptr<Modules::In::IFilePuller> createHttpSource();

namespace {

string dirName(string path) {
	auto i = path.rfind('/');
	if(i != path.npos)
		path = path.substr(0, i);
	return path + "/";
}

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

			m_dirName = dirName(cfg->url);
			m_output = addOutput<OutputDefault>();
		}

		virtual bool work() override {
			if(!m_hasPlaylist) {
				auto main = downloadPlaylist(m_playlistUrl);
				if(main.empty()) {
					m_host->log(Error, "No main playlist");
					return false;
				}

				string subUrl = main[0];

				m_chunks = downloadPlaylist(m_dirName + subUrl);
				m_hasPlaylist = true;
			}

			if(m_chunks.empty()) {
				m_host->log(Debug, "Stopping");
				return false;
			}

			auto chunkUrl = m_dirName + m_chunks[0];
			m_host->log(Debug, ("Download chunk: '" + chunkUrl + "'").c_str());
			auto chunk = m_puller->get(chunkUrl.c_str());
			m_chunks.erase(m_chunks.begin());

			auto data = m_output->getBuffer(chunk.size());
			if(chunk.size())
				memcpy(data->data().ptr, chunk.data(), chunk.size());
			m_output->emit(data);

			return true;
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
		OutputDefault* m_output = nullptr;
		bool m_hasPlaylist = false;
		string m_dirName;
		vector<string> m_chunks;
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
