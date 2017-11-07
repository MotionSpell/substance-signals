#include "apple_hls.hpp"
#include <fstream>

namespace Modules {
namespace Stream {

Apple_HLS::Apple_HLS(const std::string &m3u8Path, Type type, uint64_t segDurationInMs)
	: AdaptiveStreamingCommon(type, segDurationInMs), playlistMasterPath(m3u8Path) {
	if (segDurationInMs % 1000)
		throw error("Segment duration must be an integer number of seconds.");
}

Apple_HLS::~Apple_HLS() {
	endOfStream();
}

std::unique_ptr<Quality> Apple_HLS::createQuality() const {
	return uptr<Quality>(safe_cast<Quality>(new HLSQuality));
}

void Apple_HLS::generateManifestMaster() {
	std::stringstream playlistMaster;
	playlistMaster << "#EXTM3U" << std::endl;
	playlistMaster << "#EXT-X-VERSION:3" << std::endl;
	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		auto quality = safe_cast<HLSQuality>(qualities[i].get());
		playlistMaster << "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=" << quality->avg_bitrate_in_bps;
		switch (quality->meta->getStreamType()) {
		case AUDIO_PKT:
			playlistMaster << ",CODECS=" << "mp4a.40.5" /*TODO: quality->meta->getCodecName()*/ << std::endl;
			playlistMaster << "a_" << i;
			break;
		case VIDEO_PKT: {
			std::stringstream res; res << quality->meta->resolution[0] << "x" << quality->meta->resolution[1];
			playlistMaster << ",RESOLUTION=" << res.str() << std::endl;
			playlistMaster << "v_" << i << "_" << res.str();
			break;
		}
		default:
			assert(0);
		}
		playlistMaster << "_.m3u8" << std::endl;
	}
	std::ofstream mpl(playlistMasterPath, std::ofstream::out | std::ofstream::trunc);
	mpl << playlistMaster.str();
	mpl.close();

	if (type != Static) {
		auto out = outputManifest->getBuffer(0);
		auto metadata = std::make_shared<MetadataFile>(playlistMasterPath, PLAYLIST, "", "", timescaleToClock(segDurationInMs, 1000), 0, 1, false);
		out->setMetadata(metadata);
		outputManifest->emit(out);
	}
}

void Apple_HLS::updateManifestVoDVariants() {
#ifndef LIBAVMUXHLS
	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		auto quality = safe_cast<HLSQuality>(qualities[i].get());
		quality->segmentPaths.push_back(quality->meta->getFilename());
	}
#endif /*LIBAVMUXHLS*/
}

void Apple_HLS::generateManifestVariant() {
#ifndef LIBAVMUXHLS
	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		auto quality = safe_cast<HLSQuality>(qualities[i].get());
		quality->playlistVariant << "#EXTM3U" << std::endl;
		quality->playlistVariant << "#EXT-X-VERSION:3" << std::endl;
		quality->playlistVariant << "#EXT-X-TARGETDURATION:" << segDurationInMs / 1000 << std::endl;
		quality->playlistVariant << "#EXT-X-MEDIA-SEQUENCE:0" << std::endl;
		quality->playlistVariant << "#EXT-X-PLAYLIST-TYPE:EVENT" << std::endl;
		for (auto segPath : quality->segmentPaths) {
			quality->playlistVariant << "#EXT-X-STREAM-INF:" << segDurationInMs / 1000 << std::endl;
			quality->playlistVariant << "#EXT-X-STREAM-INF:" << segPath << std::endl;
		}
		quality->playlistVariant << "#EXT-X-ENDLIST" << std::endl;

		std::ofstream vpl;
		auto const playlistCurVariantPath = format("%s.m3u8", i);
		vpl.open(playlistCurVariantPath);
		vpl << quality->playlistVariant.str();
		vpl.close();

		auto out = outputManifest->getBuffer(0);
		auto metadata = std::make_shared<MetadataFile>(playlistCurVariantPath, PLAYLIST, "", "", timescaleToClock(segDurationInMs, 1000), 0, 1, false);
		out->setMetadata(metadata);
		outputManifest->emit(out);
	}
#endif /*LIBAVMUXHLS*/
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
