#include "lib_modules/modules.hpp"
#include "lib_media/media.hpp"
#include "pipeliner.hpp"
#include <sstream>

using namespace Modules;
using namespace Pipelines;

extern const char *g_appName;

#define DASH_SUBDIR "dash/"
#define HLS_SUBDIR  "hls/"

//#define DEBUG_MONITOR
//#define MP4_MONITOR
#define MANUAL_HLS //FIXME: see https://git.gpac-licensing.com/rbouqueau/fk-encode/issues/17 and https://git.gpac-licensing.com/rbouqueau/fk-encode/issues/18

#ifdef MANUAL_HLS
#include <fstream>
#endif

void declarePipeline(Pipeline &pipeline, const AppOptions &opt, const FormatFlags formats) {
	auto connect = [&](auto* src, auto* dst) {
		pipeline.connect(src, 0, dst, 0);
	};

	auto autoRotate = [&](const Resolution &res, bool verticalize)->Resolution {
		if (verticalize && res.height < res.width) {
			Resolution oRes;
			oRes.width = res.height;
			oRes.height = res.width;
			return oRes;
		} else {
			return res;
		}
	};

	auto createEncoder = [&](std::shared_ptr<const IMetadata> metadataDemux, const AppOptions &opt, size_t optIdx, PixelFormat &pf, bool verticalize)->IModule* {
		auto const codecType = metadataDemux->getStreamType();
		if (codecType == VIDEO_PKT) {
			Log::msg(Info, "[Encoder] Found video stream");
			Encode::LibavEncodeParams p;
			p.isLowLatency = opt.isLive;
			p.codecType = (Encode::LibavEncodeParams::VideoCodecType)opt.v[optIdx].type;
			p.res = autoRotate(opt.v[optIdx].res, verticalize);
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

	/*video is forced, audio is as passthru as possible*/
	auto createConverter = [&](std::shared_ptr<const IMetadata> metadataDemux, std::shared_ptr<const IMetadata> metadataEncoder, const PictureFormat &dstFmt)->IModule* {
		auto const codecType = metadataDemux->getStreamType();
		if (codecType == VIDEO_PKT) {
			Log::msg(Info, "[Converter] Found video stream");
			return pipeline.addModule<Transform::VideoConvert>(dstFmt);
		} else if (codecType == AUDIO_PKT) {
			Log::msg(Info, "[Converter] Found audio stream");
			PcmFormat encFmt, demuxFmt;
			libavAudioCtxConvert(&demuxFmt, safe_cast<const MetadataPktLibavAudio>(metadataDemux)->getAVCodecContext());
			auto const metaEnc = safe_cast<const MetadataPktLibavAudio>(metadataEncoder);
			auto format = PcmFormat(demuxFmt.sampleRate, demuxFmt.numChannels, demuxFmt.layout, encFmt.sampleFormat, (encFmt.numPlanes == 1) ? Interleaved : Planar);
			libavAudioCtxConvert(&encFmt, metaEnc->getAVCodecContext());
			return pipeline.addModule<Transform::AudioConvert>(format, metaEnc->getFrameSize());
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
	IPipelinedModule *hlser;
	if (formats & APPLE_HLS) {
		if (gf_mkdir(HLS_SUBDIR))
			throw std::runtime_error(format("%s - couldn't create subdir %s: please check you have sufficient rights", g_appName, HLS_SUBDIR));
		hlser = pipeline.addModule<Stream::Apple_HLS>(format("%s.m3u8", g_appName), type, opt.segmentDurationInMs);
	}
#endif
	IPipelinedModule *dasher = nullptr;
	if (formats & MPEG_DASH) {
		if (gf_mkdir(DASH_SUBDIR))
			throw std::runtime_error(format("%s - couldn't create subdir %s: please check you have sufficient rights", g_appName, DASH_SUBDIR));
		dasher = pipeline.addModule<Stream::MPEG_DASH>(format("%s%s.mpd", DASH_SUBDIR, g_appName), type, opt.segmentDurationInMs);
	}

	bool isVertical = false;
	const bool transcode = opt.v.size() > 0 ? true : false;
	if (!transcode) {
		Log::msg(Warning, "[%s] No transcode. Make passthru.", g_appName);
		if (opt.autoRotate)
			throw std::runtime_error(format("%s - cannot autorotate when no transcoding is enabled", g_appName, DASH_SUBDIR));
	}

	int numDashInputs = 0;
	for (size_t i = 0; i < demux->getNumOutputs(); ++i) {
		auto const metadataDemux = getMetadataFromOutput<MetadataPktLibav>(demux->getOutput(i));
		if (!metadataDemux) {
			Log::msg(Warning, "[%s] Unknown metadataDemux for stream %s. Ignoring.", g_appName, i);
			break;
		}

		IModule *decode = nullptr;
		if (transcode) {
			decode = pipeline.addModule<Decode::LibavDecode>(*metadataDemux);
			pipeline.connect(demux, i, decode, 0);

			if (metadataDemux->isVideo() && opt.autoRotate) {
				auto const res = getMetadataFromOutput<MetadataPktLibavVideo>(demux->getOutput(i))->getResolution();
				if (res.height > res.width) {
					isVertical = true;
				}
			}
		}

		auto const numRes = metadataDemux->isVideo() ? std::max<size_t>(opt.v.size(), 1) : 1;
		for (size_t r = 0; r < numRes; ++r, ++numDashInputs) {
			IModule *encoder = nullptr;
			if (transcode) {
				PictureFormat encoderInputPicFmt(autoRotate(opt.v[r].res, isVertical), UNKNOWN_PF);
				encoder = createEncoder(metadataDemux, opt, r, encoderInputPicFmt.format, isVertical);
				if (!encoder)
					continue;

				auto const metadataEncoder = getMetadataFromOutput<MetadataPktLibav>(encoder->getOutput(0));
				auto converter = createConverter(metadataDemux, metadataEncoder, encoderInputPicFmt);
				if (!converter)
					continue;

#ifdef DEBUG_MONITOR
				if (metadataDemux->isVideo() && r == 0) {
					auto webcamPreview = pipeline.addModule<Render::SDLVideo>();
					connect(converter, webcamPreview);
				}
#endif

				connect(decode, converter);
				connect(converter, encoder);
			}

			std::stringstream filename;
			unsigned width, height;
			if (metadataDemux->isVideo()) {
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
				filename << "audio_" << numDashInputs << "_";
			}
			if (formats & APPLE_HLS) {
				if (gf_mkdir(HLS_SUBDIR))
					throw std::runtime_error(format("%s - couldn't create subdir %s: please check you have sufficient rights", g_appName, HLS_SUBDIR));
				auto muxer = pipeline.addModule<Mux::LibavMux>(HLS_SUBDIR + filename.str(), "hls", format("-hls_time %s -hls_playlist_type event", opt.segmentDurationInMs / 1000));
				if (transcode) {
					connect(encoder, muxer);
				} else {
					pipeline.connect(demux, i, muxer, 0);
				}

#ifdef MANUAL_HLS
				if (formats & APPLE_HLS) {
					playlistMaster << "#EXT-X-STREAM-INF:PROGRAM-ID=1";
					if (metadataDemux->isVideo()) {
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
				auto muxer = pipeline.addModule<Mux::GPACMuxMP4>(DASH_SUBDIR + filename.str(), opt.segmentDurationInMs, true);
				if (transcode) {
					connect(encoder, muxer);
				} else {
					pipeline.connect(demux, i, muxer, 0);
				}

				pipeline.connect(muxer, 0, dasher, numDashInputs);
			}

#ifdef MP4_MONITOR
			//auto muxer = pipeline.addModule<Mux::GPACMuxMP4>("monitor_" + filename.str(), 0, false); //FIXME: see https://git.gpac-licensing.com/rbouqueau/fk-encode/issues/28
			auto muxer = pipeline.addModule<Mux::LibavMux>("monitor_" + filename.str(), "mp4");
			if (transcode) {
				connect(encoder, muxer);
			} else {
				pipeline.connect(demux, i, muxer, 0);
			}
#endif
		}

#ifdef MANUAL_HLS
		if (formats & APPLE_HLS) {
			std::ofstream mpl;
			mpl.open(format("%s%s.m3u8", HLS_SUBDIR, g_appName));
			mpl << playlistMaster.str();
			mpl.close();
		}
#endif
	}
}
