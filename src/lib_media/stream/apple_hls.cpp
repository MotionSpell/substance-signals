#include "apple_hls.hpp"
#include <fstream>
#include <sstream>

#ifdef _WIN32 
#include <sys/timeb.h>
#include <Winsock2.h>
#else
#include <sys/time.h>
#endif

namespace Modules {
namespace Stream {

Apple_HLS::Apple_HLS(const std::string &m3u8Dir, const std::string &m3u8Filename, Type type, uint64_t segDurationInMs, bool genVariantPlaylist, AdaptiveStreamingCommonFlags flags)
: AdaptiveStreamingCommon(type, segDurationInMs, m3u8Dir, flags), playlistMasterPath(format("%s%s", m3u8Dir, m3u8Filename)), genVariantPlaylist(genVariantPlaylist) {
	if (segDurationInMs % 1000)
		throw error("Segment duration must be an integer number of seconds.");
}

Apple_HLS::~Apple_HLS() {
	endOfStream();
}

std::unique_ptr<Quality> Apple_HLS::createQuality() const {
	return uptr<Quality>(safe_cast<Quality>(new HLSQuality));
}

void Apple_HLS::processInitSegment(Quality const * const quality, size_t index) {
	switch (quality->meta->getStreamType()) {
	case AUDIO_PKT: case VIDEO_PKT: case SUBTITLE_PKT: {
		auto out = outputSegments->getBuffer(0);
		auto const initFnSrc = getInitName(quality, index);
		auto const initFnDst = format("%s%s", manifestDir, initFnSrc);
		out->setMetadata(std::make_shared<MetadataFile>(initFnDst, SEGMENT, quality->meta->getMimeType(), quality->meta->getCodecName(), quality->meta->getDuration(), quality->meta->getSize(), quality->meta->getLatency(), quality->meta->getStartsWithRAP()));
		outputSegments->emit(out);
		break;
	}
	default: break;
	}
}

std::string Apple_HLS::getVariantPlaylistName(HLSQuality const * const quality, const std::string &subDir, size_t index) {
	switch (quality->meta->getStreamType()) {
	case AUDIO_PKT:    return format("%s%sa_%s_.m3u8", subDir, quality->prefix, index);
	case VIDEO_PKT:    return format("%s%sv_%s_%sx%s_.m3u8", subDir, quality->prefix, index, quality->meta->resolution[0], quality->meta->resolution[1]);
	case SUBTITLE_PKT: return format("%s%ss_%s", subDir, quality->prefix, index);
	default: return "";
	}
}

void Apple_HLS::generateManifestMaster() {
	if (!masterManifestIsWritten) {
		std::stringstream playlistMaster;
		playlistMaster << "#EXTM3U" << std::endl;
		playlistMaster << "#EXT-X-VERSION:3" << std::endl;
		for (size_t i = 0; i < getNumInputs() - 1; ++i) {
			auto quality = safe_cast<HLSQuality>(qualities[i].get());
			playlistMaster << "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=" << quality->avg_bitrate_in_bps;
			switch (quality->meta->getStreamType()) {
			case AUDIO_PKT: playlistMaster << ",CODECS=" << "mp4a.40.5" /*TODO: quality->meta->getCodecName()*/ << std::endl; break;
			case VIDEO_PKT: playlistMaster << ",RESOLUTION=" << quality->meta->resolution[0] << "x" << quality->meta->resolution[1] << std::endl; break;
			case SEGMENT: playlistMaster << ",RESOLUTION=" << quality->meta->resolution[0] << "x" << quality->meta->resolution[1] << std::endl; break;
			default: assert(0);
			}
			playlistMaster << getVariantPlaylistName(quality, "", i) << std::endl;
		}
		std::ofstream mpl(playlistMasterPath, std::ofstream::out | std::ofstream::trunc);
		mpl << playlistMaster.str();
		mpl.close();
		masterManifestIsWritten = true;

		if (type != Static) {
			auto out = outputManifest->getBuffer(0);
			auto metadata = std::make_shared<MetadataFile>(playlistMasterPath, PLAYLIST, "", "", timescaleToClock(segDurationInMs, 1000), 0, 1, false);
			out->setMetadata(metadata);
			outputManifest->emit(out);
		}
	}
}

void Apple_HLS::updateManifestVariants() {
	if (genVariantPlaylist) {
		for (size_t i = 0; i < getNumInputs() - 1; ++i) {
			auto quality = safe_cast<HLSQuality>(qualities[i].get());
			auto fn = quality->meta->getFilename();
			if (fn.empty()) {
				fn = getSegmentName(quality, i, std::to_string(getCurSegNum()));
			}
			auto const sepPos = fn.find_last_of(".");
			auto const ext = fn.substr(sepPos + 1);
			if (!version) {
				if (ext == "m4s") {
					version = 7;
					hasInitSeg = true;
				} else {
					version = 3;
				}
				auto const firstSegNumPos = fn.substr(0, sepPos).find_last_of("-");
				auto const firstSegNumStr = fn.substr(firstSegNumPos+1, sepPos-(firstSegNumPos+1));
				std::istringstream buffer(firstSegNumStr);
				buffer >> firstSegNum;
			}

			auto out = outputSegments->getBuffer(0);
			out->setMetadata(std::make_shared<MetadataFile>(format("%s%s", manifestDir, fn), SEGMENT, quality->meta->getMimeType(), quality->meta->getCodecName(), quality->meta->getDuration(), quality->meta->getSize(), quality->meta->getLatency(), quality->meta->getStartsWithRAP()));
			outputSegments->emit(out);

			quality->segments.push_back({ fn, startTimeInMs+totalDurationInMs });
		}

		generateManifestVariantFull(false);
	}
}

void Apple_HLS::generateManifestVariantFull(bool isLast) {
	if (genVariantPlaylist) {
		for (size_t i = 0; i < getNumInputs() - 1; ++i) {
			auto quality = safe_cast<HLSQuality>(qualities[i].get());
			quality->playlistVariant.str(std::string());
			quality->playlistVariant << "#EXTM3U" << std::endl;
			quality->playlistVariant << "#EXT-X-VERSION:" << version << std::endl;
			quality->playlistVariant << "#EXT-X-TARGETDURATION:" << (segDurationInMs + 500) / 1000 << std::endl;
			quality->playlistVariant << "#EXT-X-MEDIA-SEQUENCE:" << firstSegNum << std::endl;
			if (version >= 6) quality->playlistVariant << "#EXT-X-INDEPENDENT-SEGMENTS" << std::endl;
			if (hasInitSeg) quality->playlistVariant << "#EXT-X-MAP:URI=\"" << getInitName(quality, i) << "\"" << std::endl;
			quality->playlistVariant << "#EXT-X-PLAYLIST-TYPE:EVENT" << std::endl;

			for (auto &seg : quality->segments) {
				quality->playlistVariant << "#EXTINF:" << segDurationInMs / 1000.0 << std::endl;
				if (type != Static) {
					char cmd[100];
					struct timeval tv;
					tv.tv_sec = (long)(seg.startTimeInMs/1000);
					assert(!(tv.tv_sec & 0xFFFFFFFF00000000));
					tv.tv_usec = 0;
					time_t sec = tv.tv_sec;
					auto *tm = gmtime(&sec);
					if (!tm) {
						log(Warning, "Segment \"%s\": could not convert UTC start time %sms. Skippping PROGRAM-DATE-TIME.", seg.startTimeInMs, seg.path);
					} else {
						snprintf(cmd, sizeof(cmd), "%d-%02d-%02dT%02d:%02d:%02d.%03d+00:00", 1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, (int)(seg.startTimeInMs % 1000));
						quality->playlistVariant << "#EXT-X-PROGRAM-DATE-TIME:" << cmd << std::endl;
					}
				}

				if (seg.path.empty())
					error("HLS segment path is empty. Even when using memory mode, you must set a valid path in the metadata.");
				quality->playlistVariant << seg.path << std::endl;
			}
			quality->playlistVariant << "#EXT-X-ENDLIST" << std::endl;

			std::ofstream vpl;
			auto const playlistCurVariantPath = getVariantPlaylistName(quality, manifestDir, i);
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
