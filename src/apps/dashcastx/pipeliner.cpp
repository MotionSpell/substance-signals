#include "lib_media/media.hpp"
#include "lib_pipeline/pipeline.hpp"
#include "lib_appcommon/pipeliner.hpp"
#include <sstream>
#ifdef _WIN32
#include <direct.h> //chdir
#else
#include <unistd.h>
#endif
#include <gpac/tools.h> //gf_mkdir

using namespace Modules;
using namespace Pipelines;

extern const char *g_appName;

#define DASH_SUBDIR "dash/"

//#define DEBUG_MONITOR
//#define MP4_MONITOR

#define MAX_GOP_DURATION_IN_MS 2000

std::unique_ptr<Pipeline> buildPipeline(const IConfig &config) {
	auto opt = safe_cast<const AppOptions>(&config);
	auto pipeline = uptr(new Pipeline(opt->ultraLowLatency, opt->isLive ? 1.0 : 0.0, opt->ultraLowLatency ? Pipeline::Mono : Pipeline::OnePerModule));

	auto connect = [&](auto src, auto dst) {
		pipeline->connect(src, 0, dst, 0);
	};

	auto autoFit = [&](const Resolution &input, const Resolution &output)->Resolution {
		if (input == Resolution()) {
			return output;
		} else if (output.width == (unsigned)-1) {
			assert((input.width * output.height % input.height) == 0); //TODO: add SAR at the DASH level to handle rounding errors
			Resolution oRes((input.width * output.height) / input.height, output.height);
			Log::msg(Info, "[autoFit] Switched resolution from -1x%s to %sx%s", input.height, oRes.width, oRes.height);
			return oRes;
		} else if (output.height == (unsigned)-1) {
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

	auto createEncoder = [&](std::shared_ptr<const IMetadata> metadataDemux, bool ultraLowLatency, Encode::LibavEncode::Params::VideoCodecType videoCodecType, PictureFormat &dstFmt, unsigned bitrate, uint64_t segmentDurationInMs)->IPipelinedModule* {
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
			const int GOPDurationDivisor = 1 + (int)(segmentDurationInMs / (MAX_GOP_DURATION_IN_MS+1));
			p.GOPSize = ultraLowLatency ? 1 : (int)(segmentDurationInMs * p.frameRate) / (1000 * GOPDurationDivisor);
			if ((uint64_t)(segmentDurationInMs * p.frameRate) % (1000 * GOPDurationDivisor))
				throw std::runtime_error(format("Couldn't align GOP size (%s, divisor=%s) with segment duration (%sms). Check your input parameters.", p.GOPSize, GOPDurationDivisor, segmentDurationInMs));
			if (GOPDurationDivisor > 1) Log::msg(Info, "[Encoder] Setting GOP duration to %sms (%s frames)", segmentDurationInMs / GOPDurationDivisor, p.GOPSize);
			auto m = pipeline->addModule<Encode::LibavEncode>(Encode::LibavEncode::Video, p);
			dstFmt.format = p.pixelFormat;
			return m;
		} else if (codecType == AUDIO_PKT) {
			Log::msg(Info, "[Encoder] Found audio stream");
			PcmFormat encFmt, demuxFmt;
			libavAudioCtx2pcmConvert(safe_cast<const MetadataPktLibavAudio>(metadataDemux)->getAVCodecContext(), &demuxFmt);
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
			libavAudioCtx2pcmConvert(safe_cast<const MetadataPktLibavAudio>(metadataDemux)->getAVCodecContext(), &demuxFmt);
			auto const metaEnc = safe_cast<const MetadataPktLibavAudio>(metadataEncoder);
			auto format = PcmFormat(demuxFmt.sampleRate, demuxFmt.numChannels, demuxFmt.layout, encFmt.sampleFormat, (encFmt.numPlanes == 1) ? Interleaved : Planar);
			libavAudioCtx2pcmConvert(metaEnc->getAVCodecContext(), &encFmt);
			return pipeline->addModule<Transform::AudioConvert>(format, metaEnc->getFrameSize());
		} else {
			Log::msg(Info, "[Converter] Found unknown stream");
			return nullptr;
		}
	};

	auto createSubdir = [&]() {
		if ((gf_dir_exists(DASH_SUBDIR) == GF_FALSE) && gf_mkdir(DASH_SUBDIR))
			throw std::runtime_error(format("%s - couldn't create subdir %s: please check you have sufficient rights", g_appName, DASH_SUBDIR));
	};

	auto changeDir = [&]() {
		if (chdir(opt->workingDir.c_str()) < 0 && (gf_mkdir((char*)opt->workingDir.c_str()) || chdir(opt->workingDir.c_str()) < 0))
			throw std::runtime_error(format("%s - couldn't change dir to %s: please check the directory exists and you have sufficient rights", g_appName, opt->workingDir));
	};

	changeDir();
	auto demux = pipeline->addModule<Demux::LibavDemux>(opt->input, opt->loop);
	createSubdir();
	auto const type = opt->isLive ? Stream::AdaptiveStreamingCommon::Live : Stream::AdaptiveStreamingCommon::Static;
	auto dasher = pipeline->addModule<Stream::MPEG_DASH>(DASH_SUBDIR, format("%s.mpd", g_appName), type, opt->segmentDurationInMs, opt->segmentDurationInMs * opt->timeshiftInSegNum);

	bool isVertical = false;
	const bool transcode = opt->v.size() > 0 ? true : false;
	if (!transcode) {
		Log::msg(Warning, "[%s] No transcode. Make passthru.", g_appName);
		if (opt->autoRotate)
			throw std::runtime_error(format("%s - cannot autorotate when transcoding is disabled", g_appName, DASH_SUBDIR));
	}

	int numDashInputs = 0;
	for (size_t i = 0; i < demux->getNumOutputs(); ++i) {
		auto const metadataDemux = safe_cast<const MetadataPktLibav>(demux->getOutput(i)->getMetadata());
		if (!metadataDemux) {
			Log::msg(Warning, "[%s] Unknown metadataDemux for stream %s. Ignoring.", g_appName, i);
			continue;
		}

		IPipelinedModule *decode = nullptr;
		if (transcode) {
			decode = pipeline->addModule<Decode::LibavDecode>(*metadataDemux);
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
				Resolution inputRes = metadataDemux->isVideo() ? safe_cast<const MetadataPktLibavVideo>(demux->getOutput(i)->getMetadata())->getResolution() : Resolution();
				PictureFormat encoderInputPicFmt(autoRotate(autoFit(inputRes, opt->v[r].res), isVertical), UNKNOWN_PF);
				encoder = createEncoder(metadataDemux, opt->ultraLowLatency, (Encode::LibavEncode::Params::VideoCodecType)opt->v[r].type, encoderInputPicFmt, opt->v[r].bitrate, opt->segmentDurationInMs);
				if (!encoder)
					continue;

				auto converter = createConverter(metadataDemux, encoder->getOutput(0)->getMetadata(), encoderInputPicFmt);
				if (!converter)
					continue;

#ifdef DEBUG_MONITOR
				if (metadataDemux->isVideo() && r == 0) {
					auto webcamPreview = pipeline->addModule<Render::SDLVideo>();
					connect(converter, webcamPreview);
				}
#endif

				connect(decode, converter);
				connect(converter, encoder);
			}

			std::stringstream filename;
			unsigned width, height;
			if (metadataDemux->isVideo()) {
				auto const resolutionFromDemux = safe_cast<const MetadataPktLibavVideo>(demux->getOutput(i)->getMetadata())->getResolution();
				if (transcode) {
					auto const res = autoFit(resolutionFromDemux, opt->v[r].res);
					width = res.width;
					height = res.height;
				} else {
					width = resolutionFromDemux.width;
					height = resolutionFromDemux.height;
				}
				filename << "v_" << numDashInputs << "_" << width << "x" << height;
			} else {
				filename << "a_" << numDashInputs;
			}

			auto muxer = pipeline->addModule<Mux::GPACMuxMP4>(DASH_SUBDIR + filename.str(), opt->segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, opt->ultraLowLatency ? Mux::GPACMuxMP4::OneFragmentPerFrame : Mux::GPACMuxMP4::OneFragmentPerSegment);
			if (transcode) {
				connect(encoder, muxer);
			} else {
				pipeline->connect(demux, i, muxer, 0);
			}

			pipeline->connect(muxer, 0, dasher, numDashInputs);

#ifdef MP4_MONITOR
			//auto muxermp4 = pipeline->addModule<Mux::LibavMux>("monitor_" + filename.str(), "mp4");
			auto muxermp4 = pipeline->addModule<Mux::GPACMuxMP4>("monitor_" + filename.str());
			if (transcode) {
				connect(encoder, muxermp4);
			} else {
				pipeline->connect(demux, i, muxermp4, 0);
			}
#endif
		}
	}

	return pipeline;
}
