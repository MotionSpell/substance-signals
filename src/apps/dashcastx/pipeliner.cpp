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

class CommandExecutor : public Module {
public:
	CommandExecutor(const std::string &cmd) : cmd(cmd) {
		addInput(new Input<DataBase>(this));
	}
	virtual ~CommandExecutor() {}
	void process() {
		auto data = getInput(0)->pop();
		auto meta = safe_cast<const MetadataFile>(data->getMetadata());
		auto const str = format(cmd, meta->getFilename());
		if (system(str.c_str()) == 1) {
			Log::msg(Error, "CommandExecutor: couldn't execute '%s'", str);
		}
	}

private:
	std::string cmd;
};

/*FIXME: this should not even exist but we may need to send event when the libav muxer emits new segments*/
class LibavMuxHLS : public ModuleDynI {
public:
	LibavMuxHLS(bool isLowLatency, uint64_t segDurationInMs, const std::string &baseName, const std::string &fmt, const std::string &options = "")
	: segDuration(timescaleToClock(segDurationInMs, 1000)), segBasename(baseName) {
		if (fmt != "hls")
			error("HLS only!");
		if (isLowLatency) {
			delegate = uptr(createModule<Mux::LibavMux>(Modules::ALLOC_NUM_BLOCKS_LOW_LATENCY, baseName, fmt, options));
		} else {
			delegate = uptr(createModule<Mux::LibavMux>(Modules::ALLOC_NUM_BLOCKS_DEFAULT, baseName, fmt, options));
		}
		addInput(new Input<DataAVPacket>(this));
		outputSegmentAndManifest = addOutput<OutputDataDefault<DataAVPacket>>();
	}

	virtual ~LibavMuxHLS() {}

	void process() override {
		auto data = getInput(0)->pop();
		delegate->getInput(0)->push(data);
		delegate->process();

		const int64_t PTS = data->getTime();
		if (firstPTS == -1) {
			firstPTS = PTS;
#ifdef MANUAL_HLS
			auto out = outputSegmentAndManifest->getBuffer(0);
			auto metadata = std::make_shared<MetadataFile>(format("%s%s.m3u8", HLS_SUBDIR, g_appName), VIDEO_PKT, "", "", 0, 0, 1, false);
			out->setMetadata(metadata);
			outputSegmentAndManifest->emit(out);
#endif
		}
		if (PTS >= (segIdx + 1) * segDuration + firstPTS) {
			auto out = outputSegmentAndManifest->getBuffer(0);
			auto metadata = std::make_shared<MetadataFile>(format("%s%s.ts", segBasename, segIdx), VIDEO_PKT, "", "", segDuration, 0, 1, false);
			out->setMetadata(metadata);
			outputSegmentAndManifest->emit(out);

			out = outputSegmentAndManifest->getBuffer(0);
			metadata = std::make_shared<MetadataFile>(format("%s.m3u8", segBasename), VIDEO_PKT, "", "", 0, 0, 1, false);
			out->setMetadata(metadata);
			outputSegmentAndManifest->emit(out);

			segIdx++;
		}
	}

private:
	std::unique_ptr<Mux::LibavMux> delegate;
	OutputDataDefault<DataAVPacket> *outputSegmentAndManifest;
	int64_t firstPTS = -1, segDuration, segIdx = 0;
	std::string segBasename;
};

void declarePipeline(Pipeline &pipeline, const AppOptions &opt, const FormatFlags formats) {
	auto connect = [&](auto* src, auto* dst) {
		pipeline.connect(src, 0, dst, 0);
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

	auto createEncoder = [&](std::shared_ptr<const IMetadata> metadataDemux, bool ultraLowLatency, Encode::LibavEncodeParams::VideoCodecType videoCodecType, PictureFormat &dstFmt, unsigned bitrate, uint64_t segmentDurationInMs)->IModule* {
		auto const codecType = metadataDemux->getStreamType();
		if (codecType == VIDEO_PKT) {
			Log::msg(Info, "[Encoder] Found video stream");
			Encode::LibavEncodeParams p;
			p.isLowLatency = ultraLowLatency;
			p.codecType = videoCodecType;
			p.res = dstFmt.res;
			p.bitrate_v = bitrate;
			p.GOPSize = ultraLowLatency ? 1 : (int)(segmentDurationInMs * p.frameRate) / 1000;
			auto m = pipeline.addModule<Encode::LibavEncode>(Encode::LibavEncode::Video, p);
			dstFmt.format = p.pixelFormat;
			return m;
		} else if (codecType == AUDIO_PKT) {
			Log::msg(Info, "[Encoder] Found audio stream");
			PcmFormat encFmt, demuxFmt;
			libavAudioCtx2pcmConvert(safe_cast<const MetadataPktLibavAudio>(metadataDemux)->getAVCodecContext(), &demuxFmt);
			Encode::LibavEncodeParams p;
			p.sampleRate = demuxFmt.sampleRate;
			p.numChannels = demuxFmt.numChannels;
			return pipeline.addModule<Encode::LibavEncode>(Encode::LibavEncode::Audio, p);
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
			libavAudioCtx2pcmConvert(safe_cast<const MetadataPktLibavAudio>(metadataDemux)->getAVCodecContext(), &demuxFmt);
			auto const metaEnc = safe_cast<const MetadataPktLibavAudio>(metadataEncoder);
			auto format = PcmFormat(demuxFmt.sampleRate, demuxFmt.numChannels, demuxFmt.layout, encFmt.sampleFormat, (encFmt.numPlanes == 1) ? Interleaved : Planar);
			libavAudioCtx2pcmConvert(metaEnc->getAVCodecContext(), &encFmt);
			return pipeline.addModule<Transform::AudioConvert>(format, metaEnc->getFrameSize());
		} else {
			Log::msg(Info, "[Converter] Found unknown stream");
			return nullptr;
		}
	};

	auto demux = pipeline.addModule<Demux::LibavDemux>(opt.input);
	IPipelinedModule *command = nullptr;
	if (!opt.postCommand.empty()) {
		command = pipeline.addModule<CommandExecutor>(opt.postCommand);
	}
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
		if ((gf_dir_exists(DASH_SUBDIR) == GF_FALSE) && gf_mkdir(DASH_SUBDIR))
			throw std::runtime_error(format("%s - couldn't create subdir %s: please check you have sufficient rights", g_appName, DASH_SUBDIR));
		dasher = pipeline.addModule<Stream::MPEG_DASH>(DASH_SUBDIR, format("%s.mpd", g_appName), type, opt.segmentDurationInMs);
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
				Resolution inputRes = metadataDemux->isVideo() ? getMetadataFromOutput<MetadataPktLibavVideo>(demux->getOutput(i))->getResolution() : Resolution();
				PictureFormat encoderInputPicFmt(autoRotate(autoFit(inputRes, opt.v[r].res), isVertical), UNKNOWN_PF);
				encoder = createEncoder(metadataDemux, opt.ultraLowLatency, (Encode::LibavEncodeParams::VideoCodecType)opt.v[r].type, encoderInputPicFmt, opt.v[r].bitrate, opt.segmentDurationInMs);
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
				filename << "audio_" << numDashInputs;
			}
			if (formats & APPLE_HLS) {
				if ((gf_dir_exists(HLS_SUBDIR) == GF_FALSE) && gf_mkdir(HLS_SUBDIR))
					throw std::runtime_error(format("%s - couldn't create subdir %s: please check you have sufficient rights", g_appName, HLS_SUBDIR));
				auto muxer = pipeline.addModule<LibavMuxHLS>(opt.isLive, opt.segmentDurationInMs, HLS_SUBDIR + filename.str() + "_", "hls", format("-hls_time %s -hls_playlist_type event", opt.segmentDurationInMs / 1000));
				if (transcode) {
					connect(encoder, muxer);
				} else {
					pipeline.connect(demux, i, muxer, 0);
				}
				pipeline.connect(muxer, 0, command, 0, true);

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
					playlistMaster << filename.str() << "_.m3u8" << std::endl;
				}
#else
				pipeline.connect(muxer, 0, hlser, numDashInputs);
				pipeline.connect(hlser, 0, command, 0);
				pipeline.connect(hlser, 1, command, 0);
#endif
			}
			if (formats & MPEG_DASH) {
				auto muxer = pipeline.addModule<Mux::GPACMuxMP4>(DASH_SUBDIR + filename.str(), opt.segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, opt.ultraLowLatency ? Mux::GPACMuxMP4::OneFragmentPerFrame : Mux::GPACMuxMP4::OneFragmentPerSegment);
				if (transcode) {
					connect(encoder, muxer);
				} else {
					pipeline.connect(demux, i, muxer, 0);
				}

				pipeline.connect(muxer, 0, dasher, numDashInputs);
				pipeline.connect(muxer, 0, command, 0, true); //FIXME: segment names should be emitted by the DASHer
				pipeline.connect(dasher, 0, command, 0, true);
				pipeline.connect(dasher, 1, command, 0, true);
			}

#ifdef MP4_MONITOR
			auto muxer = pipeline.addModule<Mux::GPACMuxMP4>("monitor_" + filename.str());
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
			auto masterPlaylistPath = format("%s%s.m3u8", HLS_SUBDIR, g_appName);
			mpl.open(masterPlaylistPath);
			mpl << playlistMaster.str();
			mpl.close();

			if (!opt.postCommand.empty()) {
				auto const msg = format(opt.postCommand, masterPlaylistPath);
				if (system(msg.c_str())) //FIXME: duplicate of CommandExecutor - streamer should also take care of the muxing
					Log::msg(Warning, "[%s] Command \"%s\" failed.", g_appName, msg);
			}
		}
#endif
	}
}
