#include "lib_pipeline/pipeline.hpp"
#include "lib_utils/system_clock.hpp"
#include "lib_utils/os.hpp"
#include "config.hpp"

// modules
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/encode/libav_encode.hpp"
#include "lib_media/mux/gpac_mux_mp4.hpp"
#include "lib_media/stream/mpeg_dash.hpp"
#include "lib_media/utils/regulator.hpp"

using namespace Modules;
using namespace Pipelines;

extern const char *g_appName;

auto const DASH_SUBDIR = "dash/";
auto const MP4_MONITOR = false;
auto const MAX_GOP_DURATION_IN_MS = 2000;

Resolution autoRotate(Resolution res, bool verticalize) {
	if (verticalize && res.height < res.width) {
		Resolution oRes(res.height, res.height);
		Log::msg(Info, "[autoRotate] Switched resolution from %sx%s to %sx%s", res.width, res.height, oRes.width, oRes.height);
		return oRes;
	} else {
		return res;
	}
};

Resolution autoFit(Resolution input, Resolution output) {
	if (input == Resolution())
		return output;
	if (output.width == -1) {
		assert((input.width * output.height % input.height) == 0); //TODO: add SAR at the DASH level to handle rounding errors
		Resolution oRes((input.width * output.height) / input.height, output.height);
		Log::msg(Info, "[autoFit] Switched resolution from -1x%s to %sx%s", input.height, oRes.width, oRes.height);
		return oRes;
	} else if (output.height == -1) {
		assert((input.height * output.width % input.width) == 0); //TODO: add SAR at the DASH level to handle rounding errors
		Resolution oRes(output.width, (input.height * output.width) / input.width);
		Log::msg(Info, "[autoFit] Switched resolution from %sx-1 to %sx%s", input.width, oRes.width, oRes.height);
		return oRes;
	} else {
		return output;
	}
};

IPipelinedModule* createEncoder(Pipeline* pipeline, Metadata metadataDemux, bool ultraLowLatency, VideoCodecType videoCodecType, PictureFormat &dstFmt, int bitrate, uint64_t segmentDurationInMs) {
	auto const codecType = metadataDemux->type;
	if (codecType == VIDEO_PKT) {
		Log::msg(Info, "[Encoder] Found video stream");
		EncoderConfig p { EncoderConfig::Video };
		p.isLowLatency = ultraLowLatency;
		p.codecType = videoCodecType;
		p.res = dstFmt.res;
		p.bitrate = bitrate;

		auto const metaVideo = safe_cast<const MetadataPktLibavVideo>(metadataDemux);
		p.frameRate = metaVideo->getFrameRate();
		auto const GOPDurationDivisor = 1 + (segmentDurationInMs / (MAX_GOP_DURATION_IN_MS+1));
		p.GOPSize = ultraLowLatency ? 1 : (Fraction(segmentDurationInMs, 1000) * p.frameRate) / Fraction(GOPDurationDivisor, 1);
		if ((segmentDurationInMs * p.frameRate.num) % (1000 * GOPDurationDivisor * p.frameRate.den)) {
			Log::msg(Warning, "[%s] Couldn't align GOP size (%s/%s, divisor=%s) with segment duration (%sms). Segment duration may vary.", g_appName, p.GOPSize.num, p.GOPSize.den, GOPDurationDivisor, segmentDurationInMs);
			if ((p.frameRate.den % 1001) || ((segmentDurationInMs * p.frameRate.num * 1001) % (1000 * GOPDurationDivisor * p.frameRate.den * 1000)))
				throw std::runtime_error("GOP size checks failed. Please read previous log messages.");
		}
		if (GOPDurationDivisor > 1) Log::msg(Info, "[Encoder] Setting GOP duration to %sms (%s frames)", segmentDurationInMs / GOPDurationDivisor, (double)p.GOPSize);

		auto m = pipeline->add("Encoder", &p);
		dstFmt.format = p.pixelFormat;
		return m;
	} else if (codecType == AUDIO_PKT) {
		Log::msg(Info, "[Encoder] Found audio stream");
		auto const demuxFmt = toPcmFormat(safe_cast<const MetadataPktLibavAudio>(metadataDemux));
		EncoderConfig p { EncoderConfig::Audio };
		p.sampleRate = demuxFmt.sampleRate;
		p.numChannels = demuxFmt.numChannels;
		return pipeline->add("Encoder", &p);
	} else {
		Log::msg(Info, "[Encoder] Found unknown stream");
		return nullptr;
	}
}

/*video is forced, audio is as passthru as possible*/
IPipelinedModule* createConverter(Pipeline* pipeline, Metadata metadataDemux, Metadata metadataEncoder, const PictureFormat &dstFmt) {
	auto const codecType = metadataDemux->type;
	if (codecType == VIDEO_PKT) {
		Log::msg(Info, "[Converter] Found video stream");
		return pipeline->add("VideoConvert", &dstFmt);
	} else if (codecType == AUDIO_PKT) {
		Log::msg(Info, "[Converter] Found audio stream");
		auto const demuxFmt = toPcmFormat(safe_cast<const MetadataPktLibavAudio>(metadataDemux));
		auto const metaEnc = safe_cast<const MetadataPktLibavAudio>(metadataEncoder);
		auto const encFmt = toPcmFormat(metaEnc);
		auto format = PcmFormat(demuxFmt.sampleRate, demuxFmt.numChannels, demuxFmt.layout, encFmt.sampleFormat, (encFmt.numPlanes == 1) ? Interleaved : Planar);
		return pipeline->add("AudioConvert", nullptr, &format, metaEnc->getFrameSize());
	} else {
		Log::msg(Info, "[Converter] Found unknown stream");
		return nullptr;
	}
}

void ensureDir(std::string path) {
	if(!dirExists(path))
		mkdir(path);
}

std::unique_ptr<Pipeline> buildPipeline(const Config &cfg) {
	auto pipeline = make_unique<Pipeline>(cfg.ultraLowLatency, cfg.ultraLowLatency ? Pipeline::Mono : Pipeline::OnePerModule);

	ensureDir(cfg.workingDir);

	changeDir(cfg.workingDir);

	DemuxConfig demuxCfg;
	demuxCfg.url = cfg.input;
	demuxCfg.loop = cfg.loop;
	auto demux = pipeline->add("LibavDemux", &demuxCfg);
	auto const type = (cfg.isLive || cfg.ultraLowLatency) ? Stream::AdaptiveStreamingCommon::Live : Stream::AdaptiveStreamingCommon::Static;
	auto dasherCfg = Modules::DasherConfig { DASH_SUBDIR, format("%s.mpd", g_appName), type, (uint64_t)cfg.segmentDurationInMs, (uint64_t)cfg.segmentDurationInMs * cfg.timeshiftInSegNum};
	auto dasher = pipeline->addModuleWithHost<Stream::MPEG_DASH>(&dasherCfg);
	ensureDir(DASH_SUBDIR);

	bool isVertical = false;
	const bool transcode = cfg.v.size() > 0;
	if (!transcode) {
		Log::msg(Warning, "[%s] No transcode. Make passthru.", g_appName);
		if (cfg.autoRotate)
			throw std::runtime_error("cannot autorotate when transcoding is disabled");
	}

	int numDashInputs = 0;
	auto processElementaryStream = [&](int streamIndex) {
		auto const metadataDemux = demux->getOutputMetadata(streamIndex);
		if (!metadataDemux) {
			Log::msg(Warning, "[%s] Unknown metadataDemux for stream %s. Ignoring.", g_appName, streamIndex);
			return;
		}

		auto source = GetOutputPin(demux, streamIndex);

		if(cfg.isLive) {
			auto regulator = pipeline->addModuleWithHost<Regulator>(g_SystemClock);
			pipeline->connect(source, GetInputPin(regulator));

			source = GetOutputPin(regulator);
		}

		IPipelinedModule *decode = nullptr;
		if (transcode) {
			decode = pipeline->add("Decoder", metadataDemux->type);

			pipeline->connect(source, GetInputPin(decode));

			if (metadataDemux->isVideo() && cfg.autoRotate) {
				auto const res = safe_cast<const MetadataPktLibavVideo>(metadataDemux)->getResolution();
				if (res.height > res.width) {
					isVertical = true;
				}
			}
		}

		auto const numRes = metadataDemux->isVideo() ? std::max<int>(cfg.v.size(), 1) : 1;
		for (int r = 0; r < numRes; ++r) {
			auto compressed = source;
			if (transcode) {
				auto inputRes = metadataDemux->isVideo() ? safe_cast<const MetadataPktLibavVideo>(metadataDemux)->getResolution() : Resolution();
				auto const outputRes = autoRotate(autoFit(inputRes, cfg.v[r].res), isVertical);
				PictureFormat encoderInputPicFmt(outputRes, UNKNOWN_PF);
				auto encoder = createEncoder(pipeline.get(), metadataDemux, cfg.ultraLowLatency, (VideoCodecType)cfg.v[r].type, encoderInputPicFmt, cfg.v[r].bitrate, cfg.segmentDurationInMs);
				if (!encoder)
					return;

				auto converter = createConverter(pipeline.get(), metadataDemux, encoder->getOutputMetadata(0), encoderInputPicFmt);
				if (!converter)
					return;

				if(cfg.debugMonitor) {
					if (metadataDemux->isVideo() && r == 0) {
						auto webcamPreview = pipeline->add("SDLVideo", nullptr);
						pipeline->connect(converter, webcamPreview);
					}
				}

				pipeline->connect(decode, converter);
				pipeline->connect(converter, encoder);
				compressed = GetOutputPin(encoder);
			}

			std::string prefix;
			if (metadataDemux->isVideo()) {
				auto reso = safe_cast<const MetadataPktLibavVideo>(demux->getOutputMetadata(streamIndex))->getResolution();
				if (transcode)
					reso = autoFit(reso, cfg.v[r].res);
				prefix = Stream::AdaptiveStreamingCommon::getCommonPrefixVideo(numDashInputs, reso);
			} else {
				prefix = Stream::AdaptiveStreamingCommon::getCommonPrefixAudio(numDashInputs);
			}

			auto const subdir = DASH_SUBDIR + prefix + "/";
			ensureDir(subdir);

			auto mp4config = Mp4MuxConfig{subdir + prefix, (uint64_t)cfg.segmentDurationInMs, FragmentedSegment, cfg.ultraLowLatency ? OneFragmentPerFrame : OneFragmentPerSegment};
			auto muxer = pipeline->add("GPACMuxMP4", &mp4config);
			pipeline->connect(compressed, GetInputPin(muxer));

			pipeline->connect(muxer, GetInputPin(dasher, numDashInputs));
			++numDashInputs;

			if(MP4_MONITOR) {
				auto cfg = Mp4MuxConfig {"monitor_" + prefix };
				auto muxer = pipeline->add("GPACMuxMP4", &cfg);
				pipeline->connect(compressed, GetInputPin(muxer));
			}
		}
	};

	for (int i = 0; i < demux->getNumOutputs(); ++i) {
		processElementaryStream(i);
	}

	return pipeline;
}
