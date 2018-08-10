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

#define DASH_SUBDIR "dash/"

auto const DEBUG_MONITOR = false;
auto const MP4_MONITOR = false;

#define MAX_GOP_DURATION_IN_MS 2000

std::unique_ptr<Pipeline> buildPipeline(const Config &config) {
	auto opt = &config;
	auto pipeline = make_unique<Pipeline>(opt->ultraLowLatency, opt->ultraLowLatency ? Pipeline::Mono : Pipeline::OnePerModule);

	auto autoFit = [&](const Resolution &input, const Resolution &output)->Resolution {
		if (input == Resolution()) {
			return output;
		} else if (output.width == -1) {
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

	auto autoRotate = [&](const Resolution &res, bool verticalize)->Resolution {
		if (verticalize && res.height < res.width) {
			Resolution oRes(res.height, res.height);
			Log::msg(Info, "[autoRotate] Switched resolution from %sx%s to %sx%s", res.width, res.height, oRes.width, oRes.height);
			return oRes;
		} else {
			return res;
		}
	};

	auto createEncoder = [&](std::shared_ptr<const IMetadata> metadataDemux, bool ultraLowLatency, VideoCodecType videoCodecType, PictureFormat &dstFmt, int bitrate, uint64_t segmentDurationInMs)->IPipelinedModule* {
		auto const codecType = metadataDemux->type;
		if (codecType == VIDEO_PKT) {
			Log::msg(Info, "[Encoder] Found video stream");
			Encode::LibavEncode::Params p;
			p.isLowLatency = ultraLowLatency;
			p.codecType = videoCodecType;
			p.res = dstFmt.res;
			p.bitrate_v = bitrate;

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

			auto m = pipeline->addModule<Encode::LibavEncode>(Encode::LibavEncode::Video, p);
			dstFmt.format = p.pixelFormat;
			return m;
		} else if (codecType == AUDIO_PKT) {
			Log::msg(Info, "[Encoder] Found audio stream");
			auto const demuxFmt = toPcmFormat(safe_cast<const MetadataPktLibavAudio>(metadataDemux));
			Encode::LibavEncode::Params p;
			p.sampleRate = demuxFmt.sampleRate;
			p.numChannels = demuxFmt.numChannels;
			return pipeline->addModule<Encode::LibavEncode>(Encode::LibavEncode::Audio, p);
		} else {
			Log::msg(Info, "[Encoder] Found unknown stream");
			return nullptr;
		}
	};

	/*video is forced, audio is as passthru as possible*/
	auto createConverter = [&](std::shared_ptr<const IMetadata> metadataDemux, std::shared_ptr<const IMetadata> metadataEncoder, const PictureFormat &dstFmt)->IPipelinedModule* {
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
	};

	auto createSubdir = [&]() {
		if (!dirExists(DASH_SUBDIR))
			mkdir(DASH_SUBDIR);
	};

	if(!dirExists(opt->workingDir))
		mkdir(opt->workingDir);

	changeDir(opt->workingDir);

	auto demux = pipeline->addModuleWithHost<Demux::LibavDemux>(opt->input, opt->loop);
	createSubdir();
	auto const type = (opt->isLive || opt->ultraLowLatency) ? Stream::AdaptiveStreamingCommon::Live : Stream::AdaptiveStreamingCommon::Static;
	auto dasher = pipeline->addModule<Stream::MPEG_DASH>(DASH_SUBDIR, format("%s.mpd", g_appName), type, opt->segmentDurationInMs, opt->segmentDurationInMs * opt->timeshiftInSegNum);

	bool isVertical = false;
	const bool transcode = opt->v.size() > 0;
	if (!transcode) {
		Log::msg(Warning, "[%s] No transcode. Make passthru.", g_appName);
		if (opt->autoRotate)
			throw std::runtime_error("cannot autorotate when transcoding is disabled");
	}

	int numDashInputs = 0;
	auto processDemuxOutput = [&](const int i) {
		auto const metadataDemux = safe_cast<const MetadataPktLibav>(demux->getOutputMetadata(i));
		if (!metadataDemux) {
			Log::msg(Warning, "[%s] Unknown metadataDemux for stream %s. Ignoring.", g_appName, i);
			return;
		}

		auto sourceModule = demux;
		auto sourcePin = i;

		if(opt->isLive) {
			auto regulator = pipeline->addModule<Regulator>(g_SystemClock);
			pipeline->connect(sourceModule, sourcePin, regulator, 0);

			sourceModule = regulator;
			sourcePin = 0;
		}

		IPipelinedModule *decode = nullptr;
		if (transcode) {
			decode = pipeline->add("Decoder", metadataDemux->type);
			pipeline->connect(sourceModule, sourcePin, decode, 0);

			if (metadataDemux->isVideo() && opt->autoRotate) {
				auto const res = safe_cast<const MetadataPktLibavVideo>(demux->getOutputMetadata(i))->getResolution();
				if (res.height > res.width) {
					isVertical = true;
				}
			}
		}

		auto const numRes = metadataDemux->isVideo() ? std::max<size_t>(opt->v.size(), 1) : 1;
		for (size_t r = 0; r < numRes; ++r, ++numDashInputs) {
			IPipelinedModule *encoder = nullptr;
			if (transcode) {
				auto inputRes = metadataDemux->isVideo() ? safe_cast<const MetadataPktLibavVideo>(demux->getOutputMetadata(i))->getResolution() : Resolution();
				auto const outputRes = autoRotate(autoFit(inputRes, opt->v[r].res), isVertical);
				PictureFormat encoderInputPicFmt(outputRes, UNKNOWN_PF);
				encoder = createEncoder(metadataDemux, opt->ultraLowLatency, (VideoCodecType)opt->v[r].type, encoderInputPicFmt, opt->v[r].bitrate, opt->segmentDurationInMs);
				if (!encoder)
					return;

				auto converter = createConverter(metadataDemux, encoder->getOutputMetadata(0), encoderInputPicFmt);
				if (!converter)
					return;

				if(DEBUG_MONITOR) {
					if (metadataDemux->isVideo() && r == 0) {
						auto webcamPreview = pipeline->add("SDLVideo", nullptr);
						pipeline->connect(converter, 0, webcamPreview, 0);
					}
				}

				pipeline->connect(decode, 0, converter, 0);
				pipeline->connect(converter, 0, encoder, 0);
			}

			std::string prefix;
			if (metadataDemux->isVideo()) {
				Resolution reso;
				auto const resolutionFromDemux = safe_cast<const MetadataPktLibavVideo>(demux->getOutputMetadata(i))->getResolution();
				if (transcode) {
					reso = autoFit(resolutionFromDemux, opt->v[r].res);
				} else {
					reso = resolutionFromDemux;
				}
				prefix = Stream::AdaptiveStreamingCommon::getCommonPrefixVideo(numDashInputs, reso);
			} else {
				prefix = Stream::AdaptiveStreamingCommon::getCommonPrefixAudio(numDashInputs);
			}

			auto const subdir = DASH_SUBDIR + prefix + "/";
			if (!dirExists(subdir))
				mkdir(subdir);

			auto muxer = pipeline->addModuleWithHost<Mux::GPACMuxMP4>(Mp4MuxConfig{subdir + prefix, (uint64_t)opt->segmentDurationInMs, FragmentedSegment, opt->ultraLowLatency ? OneFragmentPerFrame : OneFragmentPerSegment});
			if (transcode) {
				pipeline->connect(encoder, 0, muxer, 0);
			} else {
				pipeline->connect(sourceModule, sourcePin, muxer, 0);
			}

			pipeline->connect(muxer, 0, dasher, numDashInputs);

			if(MP4_MONITOR) {
				auto cfg = Mp4MuxConfig {"monitor_" + prefix };
				auto muxer = pipeline->addModuleWithHost<Mux::GPACMuxMP4>(cfg);
				if (transcode) {
					pipeline->connect(encoder, 0, muxer, 0);
				} else {
					pipeline->connect(sourceModule, sourcePin, muxer, 0);
				}
			}
		}
	};

	for (int i = 0; i < demux->getNumOutputs(); ++i) {
		processDemuxOutput(i);
	}

	return pipeline;
}
