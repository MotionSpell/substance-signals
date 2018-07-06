#include "lib_pipeline/pipeline.hpp"
#include "lib_appcommon/pipeliner.hpp"
#include <sstream>
#include "lib_utils/os.hpp"

// modules
#include "lib_media/decode/decoder.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/encode/libav_encode.hpp"
#include "lib_media/render/sdl_video.hpp"
#include "lib_media/stream/mpeg_dash.hpp"
#include "lib_media/transform/audio_convert.hpp"
#include "lib_media/transform/video_convert.hpp"
#include "lib_media/mux/gpac_mux_mp4.hpp"
#include "lib_media/utils/regulator.hpp"

using namespace Modules;
using namespace Pipelines;

extern const char *g_appName;

#define DASH_SUBDIR "dash/"

auto const DEBUG_MONITOR = 0;
auto const MP4_MONITOR = 0;

#define MAX_GOP_DURATION_IN_MS 2000

std::unique_ptr<Pipeline> buildPipeline(const IConfig &config) {
	auto opt = safe_cast<const AppOptions>(&config);
	auto pipeline = make_unique<Pipeline>(opt->ultraLowLatency, opt->isLive ? 1.0 : 0.0, opt->ultraLowLatency ? Pipeline::Mono : Pipeline::OnePerModule);

	auto connect = [&](auto src, auto dst) {
		pipeline->connect(src, 0, dst, 0);
	};

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
		auto const codecType = metadataDemux->getStreamType();
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
			if (GOPDurationDivisor > 1) Log::msg(Info, "[Encoder] Setting GOP duration to %sms (%s/%s frames)", segmentDurationInMs / GOPDurationDivisor, p.GOPSize.num, p.GOPSize.den);

			auto m = pipeline->addModule<Encode::LibavEncode>(Encode::LibavEncode::Video, p);
			dstFmt.format = p.pixelFormat;
			return m;
		} else if (codecType == AUDIO_PKT) {
			Log::msg(Info, "[Encoder] Found audio stream");
			PcmFormat encFmt, demuxFmt;
			libavAudioCtx2pcmConvert(safe_cast<const MetadataPktLibavAudio>(metadataDemux), &demuxFmt);
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
		auto const codecType = metadataDemux->getStreamType();
		if (codecType == VIDEO_PKT) {
			Log::msg(Info, "[Converter] Found video stream");
			return pipeline->addModule<Transform::VideoConvert>(dstFmt);
		} else if (codecType == AUDIO_PKT) {
			Log::msg(Info, "[Converter] Found audio stream");
			PcmFormat encFmt, demuxFmt;
			libavAudioCtx2pcmConvert(safe_cast<const MetadataPktLibavAudio>(metadataDemux), &demuxFmt);
			auto const metaEnc = safe_cast<const MetadataPktLibavAudio>(metadataEncoder);
			auto format = PcmFormat(demuxFmt.sampleRate, demuxFmt.numChannels, demuxFmt.layout, encFmt.sampleFormat, (encFmt.numPlanes == 1) ? Interleaved : Planar);
			libavAudioCtx2pcmConvert(metaEnc, &encFmt);
			return pipeline->addModule<Transform::AudioConvert>(format, metaEnc->getFrameSize());
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

	auto demux = pipeline->addModule<Demux::LibavDemux>(opt->input, opt->loop);
	createSubdir();
	auto const type = (opt->isLive || opt->ultraLowLatency) ? Stream::AdaptiveStreamingCommon::Live : Stream::AdaptiveStreamingCommon::Static;
	auto dasher = pipeline->addModule<Stream::MPEG_DASH>(DASH_SUBDIR, format("%s.mpd", g_appName), type, opt->segmentDurationInMs, opt->segmentDurationInMs * opt->timeshiftInSegNum);

	bool isVertical = false;
	const bool transcode = opt->v.size() > 0;
	if (!transcode) {
		Log::msg(Warning, "[%s] No transcode. Make passthru.", g_appName);
		if (opt->autoRotate)
			throw std::runtime_error(format("cannot autorotate when transcoding is disabled", DASH_SUBDIR));
	}

	int numDashInputs = 0;
	for (int i = 0; i < demux->getNumOutputs(); ++i) {
		auto const metadataDemux = safe_cast<const MetadataPktLibav>(demux->getOutput(i)->getMetadata());
		if (!metadataDemux) {
			Log::msg(Warning, "[%s] Unknown metadataDemux for stream %s. Ignoring.", g_appName, i);
			continue;
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
			decode = pipeline->addModule<Decode::Decoder>(metadataDemux->getStreamType());
			pipeline->connect(demux, i, decode, 0);

			if (metadataDemux->isVideo() && opt->autoRotate) {
				auto const res = safe_cast<const MetadataPktLibavVideo>(demux->getOutput(i)->getMetadata())->getResolution();
				if (res.height > res.width) {
					isVertical = true;
				}
			}
		}

		auto const numRes = metadataDemux->isVideo() ? std::max<size_t>(opt->v.size(), 1) : 1;
		for (size_t r = 0; r < numRes; ++r, ++numDashInputs) {
			IPipelinedModule *encoder = nullptr;
			if (transcode) {
				auto inputRes = metadataDemux->isVideo() ? safe_cast<const MetadataPktLibavVideo>(demux->getOutput(i)->getMetadata())->getResolution() : Resolution();
				PictureFormat encoderInputPicFmt(autoRotate(autoFit(inputRes, opt->v[r].res), isVertical), UNKNOWN_PF);
				encoder = createEncoder(metadataDemux, opt->ultraLowLatency, (VideoCodecType)opt->v[r].type, encoderInputPicFmt, opt->v[r].bitrate, opt->segmentDurationInMs);
				if (!encoder)
					continue;

				auto converter = createConverter(metadataDemux, encoder->getOutput(0)->getMetadata(), encoderInputPicFmt);
				if (!converter)
					continue;

				if(DEBUG_MONITOR) {
					if (metadataDemux->isVideo() && r == 0) {
						auto webcamPreview = pipeline->addModule<Modules::Render::SDLVideo>();
						connect(converter, webcamPreview);
					}
				}

				connect(decode, converter);
				connect(converter, encoder);
			}

			std::string prefix;
			Resolution reso;
			if (metadataDemux->isVideo()) {
				auto const resolutionFromDemux = safe_cast<const MetadataPktLibavVideo>(demux->getOutput(i)->getMetadata())->getResolution();
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

			auto muxer = pipeline->addModule<Mux::GPACMuxMP4>(subdir + prefix, opt->segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, opt->ultraLowLatency ? Mux::GPACMuxMP4::OneFragmentPerFrame : Mux::GPACMuxMP4::OneFragmentPerSegment);
			if (transcode) {
				connect(encoder, muxer);
			} else {
				pipeline->connect(demux, i, muxer, 0);
			}

			pipeline->connect(muxer, 0, dasher, numDashInputs);

			if(MP4_MONITOR) {
				//auto muxermp4 = pipeline->addModule<Mux::LibavMux>("monitor_" + prefix.str(), "mp4");
				auto muxermp4 = pipeline->addModule<Mux::GPACMuxMP4>("monitor_" + prefix);
				if (transcode) {
					connect(encoder, muxermp4);
				} else {
					pipeline->connect(demux, i, muxermp4, 0);
				}
			}
		}
	}

	return pipeline;
}
