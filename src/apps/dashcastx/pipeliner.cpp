#include "lib_modules/modules.hpp"
#include "lib_media/media.hpp"
#include "pipeliner.hpp"
#include <sstream>

using namespace Modules;
using namespace Pipelines;

#define DEBUG_MONITOR

void declarePipeline(Pipeline &pipeline, const dashcastXOptions &opt) {
	auto connect = [&](auto* src, auto* dst) {
		pipeline.connect(src, 0, dst, 0);
	};

	auto createEncoder = [&](std::shared_ptr<const IMetadata> metadata, const dashcastXOptions &opt, PixelFormat &pf, size_t i)->IModule* {
		auto const codecType = metadata->getStreamType();
		if (codecType == VIDEO_PKT) {
			Log::msg(Info, "[Encoder] Found video stream");
			Encode::LibavEncodeParams p;
			p.isLowLatency = opt.isLive;
			p.res = opt.v[i].res;
			p.bitrate_v = opt.v[i].bitrate;
			auto m = pipeline.addModule<Encode::LibavEncode>(Encode::LibavEncode::Video, p);
			pf = p.pixelFormat;
			return m;
		}
		else if (codecType == AUDIO_PKT) {
			Log::msg(Info, "[Encoder] Found audio stream");
			return pipeline.addModule<Encode::LibavEncode>(Encode::LibavEncode::Audio);
		}
		else {
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
	auto dasher = pipeline.addModule<Modules::Stream::MPEG_DASH>("dashcastx.mpd",
	                                 opt.isLive ? Modules::Stream::MPEG_DASH::Live : Modules::Stream::MPEG_DASH::Static, opt.segmentDuration);

	const bool transcode = opt.v.size() > 0 ? true : false;
	if (!transcode) {
		Log::msg(Warning, "[DashcastX] No transcode. Make passthru.");
	}

	int numDashInputs = 0;
	for (size_t i = 0; i < demux->getNumOutputs(); ++i) {
		auto const metadata = getMetadataFromOutput<MetadataPktLibav>(demux->getOutput(i));
		if (!metadata) {
			Log::msg(Warning, "[DashcastX] Unknown metadata for stream %s. Ignoring.", i);
			break;
		}

		IModule *decode = nullptr;
		if (transcode) {
			decode = pipeline.addModule<Decode::LibavDecode>(*metadata);
			pipeline.connect(demux, i, decode, 0);
		}

		auto const numRes = metadata->isVideo() ? std::max<size_t>(opt.v.size(), 1) : 1;
		for (size_t r = 0; r < numRes; ++r) {
			IModule *encoder = nullptr;
			if (transcode) {
				PictureFormat picFmt(opt.v[r].res, UNKNOWN_PF);
				encoder = createEncoder(metadata, opt, picFmt.format, r);
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
			filename << numDashInputs;
			auto muxer = pipeline.addModule<Mux::GPACMuxMP4>(filename.str(), opt.segmentDuration, true);
			if (transcode) {
				connect(encoder, muxer);
			} else {
				pipeline.connect(demux, i, muxer, 0);
			}

			pipeline.connect(muxer, 0, dasher, numDashInputs);
			numDashInputs++;
		}
	}
}
