#include "lib_modules/modules.hpp"
#include "lib_media/media.hpp"
#include "pipeliner.hpp"
#include <sstream>

using namespace Modules;
using namespace Pipelines;

extern const char *g_appName;

//#define DEBUG_MONITOR

#define MANUAL_HLS //FIXME: see https://git.gpac-licensing.com/rbouqueau/fk-encode/issues/17 and https://git.gpac-licensing.com/rbouqueau/fk-encode/issues/18
#ifdef MANUAL_HLS
#include <fstream>
#endif

void declarePipeline(Pipeline &pipeline, const AppOptions &opt, const FormatFlags formats) {
	auto connect = [&](auto* src, auto* dst) {
		pipeline.connect(src, 0, dst, 0);
	};

	auto createEncoder = [&](std::shared_ptr<const IMetadata> metadata, const AppOptions &opt, size_t optIdx, PixelFormat &pf)->IModule* {
		auto const codecType = metadata->getStreamType();
		if (codecType == VIDEO_PKT) {
			Log::msg(Info, "[Encoder] Found video stream");
			Encode::LibavEncodeParams p;
			p.isLowLatency = opt.isLive;
			p.codecType = (Encode::LibavEncodeParams::VideoCodecType)opt.v[optIdx].type;
			p.res = opt.v[optIdx].res;
			p.bitrate_v = opt.v[optIdx].bitrate;
			auto m = pipeline.addModule<Encode::LibavEncode>(Encode::LibavEncode::Video, p);
			pf = p.pixelFormat;
			return m;
		} else if (codecType == AUDIO_PKT) {
			Log::msg(Info, "[Encoder] Found audio stream");
			return pipeline.addModule<Encode::LibavEncode>(Encode::LibavEncode::Audio);
		} else {
			Log::msg(Info, "[Encoder] Found unknown stream");
			return nullptr;
		}
	};

	auto createConverter = [&](std::shared_ptr<const IMetadata> metadata, const PictureFormat &dstFmt)->IModule* {
		auto const codecType = metadata->getStreamType();
		if (codecType == VIDEO_PKT) {
			Log::msg(Info, "[Converter] Found video stream");
			return pipeline.addModule<Transform::VideoConvert>(dstFmt);
		} else if (codecType == AUDIO_PKT) {
			Log::msg(Info, "[Converter] Found audio stream");
			auto format = PcmFormat(44100, 2, AudioLayout::Stereo, AudioSampleFormat::F32, AudioStruct::Planar);
			return pipeline.addModule<Transform::AudioConvert>(format);
		} else {
			Log::msg(Info, "[Converter] Found unknown stream");
			return nullptr;
		}
	};

	auto demux = pipeline.addModule<Demux::LibavDemux>(opt.url);

	auto const type = opt.isLive ? Stream::AdaptiveStreamingCommon::Live : Stream::AdaptiveStreamingCommon::Static;
#ifdef MANUAL_HLS
		std::stringstream playlistMaster;
	if (formats & APPLE_HLS) {
		playlistMaster.clear();
		playlistMaster << "#EXTM3U" << std::endl;
		playlistMaster << "#EXT-X-VERSION:3" << std::endl;
	}
#else
	auto hlser = pipeline.addModule<Stream::Apple_HLS>(format("%s.m3u8", g_appName), type, opt.segmentDurationInMs);
#endif
	auto dasher = pipeline.addModule<Stream::MPEG_DASH>(format("%s.mpd", g_appName), type, opt.segmentDurationInMs);

	const bool transcode = opt.v.size() > 0 ? true : false;
	if (!transcode) {
		Log::msg(Warning, "[%s] No transcode. Make passthru.", g_appName);
	}

	int numDashInputs = 0;
	for (size_t i = 0; i < demux->getNumOutputs(); ++i) {
		auto const metadata = getMetadataFromOutput<MetadataPktLibav>(demux->getOutput(i));
		if (!metadata) {
			Log::msg(Warning, "[%s] Unknown metadata for stream %s. Ignoring.", g_appName, i);
			break;
		}

		IModule *decode = nullptr;
		if (transcode) {
			decode = pipeline.addModule<Decode::LibavDecode>(*metadata);
			pipeline.connect(demux, i, decode, 0);
		}

		auto const numRes = metadata->isVideo() ? std::max<size_t>(opt.v.size(), 1) : 1;
		for (size_t r = 0; r < numRes; ++r, ++numDashInputs) {
			IModule *encoder = nullptr;
			if (transcode) {
				PictureFormat picFmt(opt.v[r].res, UNKNOWN_PF);
				encoder = createEncoder(metadata, opt, r, picFmt.format);
				if (!encoder)
					continue;

				auto converter = createConverter(metadata, picFmt);
				if (!converter)
					continue;

#ifdef DEBUG_MONITOR
				if (metadata->isVideo() && r == 0) {
					auto webcamPreview = pipeline.addModule<Render::SDLVideo>();
					connect(converter, webcamPreview);
				}
#endif

				connect(decode, converter);
				connect(converter, encoder);
			}

			std::stringstream filename;
			unsigned width, height;
			if (metadata->isVideo()) {
				if (transcode) {
					width = opt.v[r].res.width;
					height = opt.v[r].res.height;
				} else {
					auto const resolutionFromDemux = getMetadataFromOutput<MetadataPktLibavVideo>(demux->getOutput(i))->getResolution();
					width = resolutionFromDemux.width;
					height = resolutionFromDemux.height;
				}
				filename << "video_" << numDashInputs << "_" << width << "x" << height;
			} else {
				filename << "audio";
			}
			if (formats & APPLE_HLS) {
				auto muxer = pipeline.addModule<Mux::LibavMux>(filename.str(), "hls", format("-hls_time %s -hls_playlist_type event", opt.segmentDurationInMs / 1000));
				if (transcode) {
					connect(encoder, muxer);
				} else {
					pipeline.connect(demux, i, muxer, 0);
				}

#ifdef MANUAL_HLS
				if (formats & APPLE_HLS) {
					playlistMaster << "#EXT-X-STREAM-INF:PROGRAM-ID=1";
					if (metadata->isVideo()) {
						playlistMaster << ",RESOLUTION=" << width << "x" << height;
						if (!opt.v.empty()) {
							playlistMaster << ",BANDWIDTH=" << opt.v[r].bitrate;
						}
					} else {
						playlistMaster << ",CODECS=\"mp4a.40.5\",BANDWIDTH=128000"; //FIXME: hardcoded
					}
					playlistMaster << std::endl;
					playlistMaster << filename.str() << ".m3u8" << std::endl;
				}
#else
				pipeline.connect(muxer, 0, hlser, numDashInputs);
#endif
			}
			if (formats & MPEG_DASH) {
				auto muxer = pipeline.addModule<Mux::GPACMuxMP4>(filename.str(), opt.segmentDurationInMs, true);
				if (transcode) {
					connect(encoder, muxer);
				} else {
					pipeline.connect(demux, i, muxer, 0);
				}

				pipeline.connect(muxer, 0, dasher, numDashInputs);
			}
		}

#ifdef MANUAL_HLS
		if (formats & APPLE_HLS) {
			std::ofstream mpl;
			mpl.open(format("%s.m3u8", g_appName));
			mpl << playlistMaster.str();
			mpl.close();
		}
#endif
	}
}
