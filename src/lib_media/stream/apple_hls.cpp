#include "apple_hls.hpp"
#include <fstream>

namespace Modules {
namespace Stream {

Apple_HLS::Apple_HLS(const std::string &m3u8Path, Type type, uint64_t segDurationInMs)
	: AdaptiveStreamingCommon(type, segDurationInMs), playlistMasterPath(m3u8Path) {
	if (segDurationInMs % 1000)
		throw error("segment duration must be an integer number of seconds.");
}

Apple_HLS::~Apple_HLS() {
	endOfStream();
}

std::unique_ptr<Quality> Apple_HLS::createQuality() const {
	return uptr<Quality>(safe_cast<Quality>(new HLSQuality));
}

void Apple_HLS::generateManifestMaster() {
	playlistMaster.clear();
	playlistMaster << "#EXTM3U" << std::endl;
	playlistMaster << "#EXT-X-VERSION:3" << std::endl;
	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		auto quality = safe_cast<HLSQuality>(qualities[i].get());
		playlistMaster << "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=" << quality->avg_bitrate_in_bps << ",RESOLUTION=" << quality->meta->resolution[0] << "x" << quality->meta->resolution[1] << std::endl;
		playlistMaster << i << ".m3u8" << std::endl;
	}

	if (type == Live) {
		std::ofstream mpl;
		mpl.open(playlistMasterPath);
		mpl << playlistMaster.str();
		mpl.close();
	}
}

void Apple_HLS::updateManifestVoDVariants() {
	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		auto quality = safe_cast<HLSQuality>(qualities[i].get());
		quality->segmentPaths.push_back(quality->meta->getFilename());
	}
}

void Apple_HLS::generateManifestVariant() {
	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		auto quality = safe_cast<HLSQuality>(qualities[i].get());
		quality->playlistVariant << "#EXTM3U" << std::endl;
		quality->playlistVariant << "#EXT-X-VERSION:3" << std::endl;
		quality->playlistVariant << "#EXT-X-TARGETDURATION:" << segDurationInMs / 1000 << std::endl;
		for (auto segPath : quality->segmentPaths) {
			quality->playlistVariant << "#EXT-X-STREAM-INF:" << segDurationInMs / 1000 << std::endl;
			quality->playlistVariant << "#EXT-X-STREAM-INF:" << segPath << std::endl;
		}
		quality->playlistVariant << "#EXT-X-ENDLIST" << std::endl;
		quality->playlistVariant << i << ".m3u8" << std::endl;

		std::ofstream vpl;
		vpl.open(format("%s.m3u8", i));
		vpl << quality->playlistVariant.str();
		vpl.close();
	}
}

void Apple_HLS::generateManifest() {
	generateManifestMaster();
	updateManifestVoDVariants();
}

void Apple_HLS::finalizeManifest() {
	generateManifestVariant();
}

}
}
