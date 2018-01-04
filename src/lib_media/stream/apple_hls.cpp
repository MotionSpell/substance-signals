#include "apple_hls.hpp"
#include <fstream>

namespace Modules {
namespace Stream {

Apple_HLS::Apple_HLS(const std::string &m3u8Dir, const std::string &m3u8Filename, Type type, uint64_t segDurationInMs, bool genVariantPlaylist)
: AdaptiveStreamingCommon(type, segDurationInMs), m3u8Dir(m3u8Dir), playlistMasterPath(format("%s%s", m3u8Dir, m3u8Filename)), genVariantPlaylist(genVariantPlaylist) {
	if (segDurationInMs % 1000)
		throw error("Segment duration must be an integer number of seconds.");
}

Apple_HLS::~Apple_HLS() {
	endOfStream();
}

std::unique_ptr<Quality> Apple_HLS::createQuality() const {
	return uptr<Quality>(safe_cast<Quality>(new HLSQuality));
}

std::string Apple_HLS::getVariantPlaylistName(HLSQuality const * const quality, const std::string &subDir, size_t index) {
	switch (quality->meta->getStreamType()) {
	case AUDIO_PKT:    return format("%sa_%s_.m3u8", subDir, index);
	case VIDEO_PKT:    return format("%sv_%s_%sx%s_.m3u8", subDir, index, quality->meta->resolution[0], quality->meta->resolution[1]);
	case SUBTITLE_PKT: return format("%ss_%s_.m3u8", subDir, index);
	default: return "";
	}
}

void Apple_HLS::generateManifestMaster() {
	std::stringstream playlistMaster;
	playlistMaster << "#EXTM3U" << std::endl;
	playlistMaster << "#EXT-X-VERSION:3" << std::endl;
	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		auto quality = safe_cast<HLSQuality>(qualities[i].get());
		playlistMaster << "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=" << quality->avg_bitrate_in_bps;
		switch (quality->meta->getStreamType()) {
		case AUDIO_PKT: playlistMaster << ",CODECS=" << "mp4a.40.5" /*TODO: quality->meta->getCodecName()*/ << std::endl; break;
		case VIDEO_PKT: playlistMaster << ",RESOLUTION=" << quality->meta->resolution[0] << "x" << quality->meta->resolution[1] << std::endl; break;
		default: assert(0);
		}
		playlistMaster << getVariantPlaylistName(quality, "", i) << std::endl;
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

void Apple_HLS::updateManifestVariants() {
	if (genVariantPlaylist) {
		for (size_t i = 0; i < getNumInputs() - 1; ++i) {
			auto quality = safe_cast<HLSQuality>(qualities[i].get());
			quality->segmentPaths.push_back(quality->meta->getFilename());
		}

		generateManifestVariantFull(false);
	}
}

void Apple_HLS::generateManifestVariantFull(bool isLast) {
	if (genVariantPlaylist) {
		for (size_t i = 0; i < getNumInputs() - 1; ++i) {
			auto quality = safe_cast<HLSQuality>(qualities[i].get());
			quality->playlistVariant.clear();
			quality->playlistVariant << "#EXTM3U" << std::endl;
			quality->playlistVariant << "#EXT-X-VERSION:3" << std::endl;
			quality->playlistVariant << "#EXT-X-TARGETDURATION:" << (segDurationInMs + 500) / 1000 << std::endl;
			quality->playlistVariant << "#EXT-X-MEDIA-SEQUENCE:0" << std::endl;
			quality->playlistVariant << "#EXT-X-PLAYLIST-TYPE:EVENT" << std::endl;
			for (auto segPath : quality->segmentPaths) {
				quality->playlistVariant << "#EXTINF:" << segDurationInMs / 1000.0 << std::endl;
				quality->playlistVariant << segPath << std::endl;
			}
			quality->playlistVariant << "#EXT-X-ENDLIST" << std::endl;

			std::ofstream vpl;
			auto const playlistCurVariantPath = getVariantPlaylistName(quality, m3u8Dir, i);
			vpl.open(playlistCurVariantPath, std::ofstream::out | std::ofstream::trunc);
			vpl << quality->playlistVariant.str();
			vpl.close();

			auto out = outputManifest->getBuffer(0);
			auto metadata = std::make_shared<MetadataFile>(playlistCurVariantPath, PLAYLIST, "", "", timescaleToClock(segDurationInMs, 1000), 0, 1, false);
			out->setMetadata(metadata);
			outputManifest->emit(out);
		}
	}
}

void Apple_HLS::generateManifest() {
	generateManifestMaster();
	updateManifestVariants();
}

void Apple_HLS::finalizeManifest() {
	generateManifestVariantFull(true);
}

}
}
