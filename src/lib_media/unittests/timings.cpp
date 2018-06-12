#include "tests/tests.hpp"
#include "lib_media/decode/decoder.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/demux/gpac_demux_mp4_simple.hpp"
#include "lib_media/encode/libav_encode.hpp"
#include "lib_media/mux/gpac_mux_mp4.hpp"
#include "lib_media/transform/audio_convert.hpp"
#include "lib_media/transform/audio_gap_filler.hpp"
#include "lib_media/transform/restamp.hpp"
#include "lib_media/transform/video_convert.hpp"
#include "lib_media/utils/recorder.hpp"
#include "lib_modules/modules.hpp"
#include <cmath>

using namespace Tests;
using namespace Modules;

template<typename T>
void checkTimestampsMux(const std::vector<int64_t> &timesIn, const std::vector<int64_t> &timesOut, std::unique_ptr<IModule> mux) {
	Encode::LibavEncode::Params p;
	p.frameRate.num = 1;
	auto picture = make_shared<PictureYUV420P>(VIDEO_RESOLUTION);
	auto encode = create<Encode::LibavEncode>(Encode::LibavEncode::Video, p);
	ConnectOutputToInput(encode->getOutput(0), mux->getInput(0));
	for (size_t i = 0; i < timesIn.size(); ++i) {
		picture->setMediaTime(timesIn[i]);
		encode->process(picture);
	}
	encode->flush();
	mux->flush();

	size_t i = 0;
	auto onFrame = [&](Data data) {
		if (i < timesOut.size()) {
			ASSERT(data->getMediaTime() == timesOut[i]);
		}
		i++;
	};
	auto demux = create<T>("out/random_ts.mp4");
	ConnectOutput(demux.get(), onFrame);
	demux->process(nullptr);
	ASSERT(i == timesOut.size());
}

template<typename T>
void checkTimestamps(const std::vector<int64_t> &timesIn, const std::vector<int64_t> &timesOut) {
	checkTimestampsMux<T>(timesIn, timesOut, create<Mux::GPACMuxMP4>("out/random_ts"));
	checkTimestampsMux<T>(timesIn, timesOut, create<Mux::GPACMuxMP4>("out/random_ts", 0, Mux::GPACMuxMP4::NoSegment, Mux::GPACMuxMP4::NoFragment, Mux::GPACMuxMP4::ExactInputDur));
}

unittest("timestamps start at random values (LibavDemux)") {
	const int64_t interval = (int64_t)IClock::Rate;
	const std::vector<int64_t> correct = { interval, 2 * interval, 3 * interval };
	const std::vector<int64_t> incorrect = { 0 };
	checkTimestamps<Demux::LibavDemux>(correct, incorrect);
}

unittest("timestamps start at random values (GPACDemuxMP4Simple)") {
	const int64_t interval = (int64_t)IClock::Rate;
	const std::vector<int64_t> correct = { interval, 2 * interval, 3 * interval };
	checkTimestamps<Demux::GPACDemuxMP4Simple>(correct, correct);
}

unittest("timestamps start at a negative value (LibavDemux)") {
	const int64_t interval = (int64_t)IClock::Rate;
	const std::vector<int64_t> correct = { -interval, 0, interval };
	const std::vector<int64_t> incorrect = { 0 };
	checkTimestamps<Demux::LibavDemux>(correct, incorrect);
}

unittest("timestamps start at a negative value (GPACDemuxMP4Simple)") {
	const int64_t interval = (int64_t)IClock::Rate;
	const std::vector<int64_t> correct = { -interval, 0, interval };
	checkTimestamps<Demux::GPACDemuxMP4Simple>(correct, correct);
}

unittest("transcoder with reframers: test a/v sync recovery") {
	ScopedLogLevel lev(Quiet); // some corrupt frames will be detected
	const int64_t maxDurIn180k = 2 * IClock::Rate;
	const size_t bufferSize = (maxDurIn180k * 1000) / (20 * IClock::Rate);

	struct Gapper : public ModuleS {
		Gapper() {
			addInput(new Input<DataBase>(this));
			output = addOutput<OutputDefault>();
		}
		void process(Data data) override {
			if ((i++ % 5) && (data->getMediaTime() < maxDurIn180k)) {
				output->emit(data);
			}
		}
		uint64_t i = 0;
		OutputDefault *output;
	};

	auto createEncoder = [&](std::shared_ptr<const IMetadata> metadataDemux, PictureFormat &dstFmt)->std::unique_ptr<IModule> {
		auto const codecType = metadataDemux->getStreamType();
		if (codecType == VIDEO_PKT) {
			Encode::LibavEncode::Params p;
			auto m = createModule<Encode::LibavEncode>(bufferSize, g_DefaultClock, Encode::LibavEncode::Video, p);
			dstFmt.format = p.pixelFormat;
			return std::move(m);
		} else if (codecType == AUDIO_PKT) {
			PcmFormat encFmt, demuxFmt;
			libavAudioCtx2pcmConvert(safe_cast<const MetadataPktLibavAudio>(metadataDemux), &demuxFmt);
			Encode::LibavEncode::Params p;
			p.sampleRate = demuxFmt.sampleRate;
			p.numChannels = demuxFmt.numChannels;
			return createModule<Encode::LibavEncode>(bufferSize, g_DefaultClock, Encode::LibavEncode::Audio, p);
		} else
			throw std::runtime_error("[Converter] Found unknown stream");
	};
	auto createConverter = [&](std::shared_ptr<const IMetadata> metadataDemux, std::shared_ptr<const IMetadata> metadataEncoder, const PictureFormat &dstFmt)->std::unique_ptr<IModule> {
		auto const codecType = metadataDemux->getStreamType();
		if (codecType == VIDEO_PKT) {
			return create<Transform::VideoConvert>(dstFmt);
		} else if (codecType == AUDIO_PKT) {
			PcmFormat encFmt, demuxFmt;
			libavAudioCtx2pcmConvert(safe_cast<const MetadataPktLibavAudio>(metadataDemux), &demuxFmt);
			auto const metaEnc = safe_cast<const MetadataPktLibavAudio>(metadataEncoder);
			auto format = PcmFormat(demuxFmt.sampleRate, demuxFmt.numChannels, demuxFmt.layout, encFmt.sampleFormat, (encFmt.numPlanes == 1) ? Interleaved : Planar);
			libavAudioCtx2pcmConvert(metaEnc, &encFmt);
			return create<Transform::AudioConvert>(format, metaEnc->getFrameSize());
		} else
			throw std::runtime_error("[Converter] Found unknown stream");
	};

	auto demux = create<Demux::LibavDemux>("data/beepbop.mp4");
	std::vector<std::unique_ptr<IModule>> modules;
	std::vector<std::unique_ptr<Utils::Recorder>> recorders;
	for (int i = 0; i < demux->getNumOutputs(); ++i) {
		auto const metadataDemux = safe_cast<const MetadataPktLibav>(demux->getOutput(i)->getMetadata());
		if (!metadataDemux->isVideo())
			continue;

		auto gapper = create<Gapper>();
		ConnectOutputToInput(demux->getOutput(i), gapper->getInput(0));
		auto decoder = create<Decode::Decoder>(metadataDemux->getStreamType());
		ConnectOutputToInput(gapper->getOutput(0), decoder->getInput(0));

		auto inputRes = safe_cast<const MetadataPktLibavVideo>(demux->getOutput(i)->getMetadata())->getResolution();
		PictureFormat encoderInputPicFmt(inputRes, UNKNOWN_PF);
		auto encoder = createEncoder(metadataDemux, encoderInputPicFmt);
		auto converter = createConverter(metadataDemux, encoder->getOutput(0)->getMetadata(), encoderInputPicFmt);
		ConnectOutputToInput(decoder->getOutput(0), converter->getInput(0));
		ConnectOutputToInput(converter->getOutput(0), encoder->getInput(0));

		auto recorder = create<Utils::Recorder>();
		ConnectOutputToInput(encoder->getOutput(0), recorder->getInput(0));
		recorders.push_back(std::move(recorder));
		modules.push_back(std::move(gapper));
		modules.push_back(std::move(decoder));
		modules.push_back(std::move(converter));
		modules.push_back(std::move(encoder));
	}

	demux->process(nullptr);
	for (auto &m : modules) {
		m->flush();
	}

	for (size_t g = 0; g < recorders.size(); ++g) {
		recorders[g]->process(nullptr);
		int64_t lastMediaTime = 0;
		while (auto data = recorders[g]->pop()) {
			Log::msg(Debug, "recv[%s] %s", g, data->getMediaTime());
			lastMediaTime = data->getMediaTime();
		}
		ASSERT(llabs(maxDurIn180k - lastMediaTime) < maxDurIn180k / 30);
	}
}

unittest("restamp: passthru with offsets") {
	auto const time = 10001LL;
	int64_t expected = 0;
	auto onFrame = [&](Data data) {
		ASSERT(data->getMediaTime() == expected);
	};
	auto data = make_shared<DataRaw>(0);

	data->setMediaTime(time);
	auto restamp = create<Transform::Restamp>(Transform::Restamp::Reset);
	ConnectOutput(restamp.get(), onFrame);
	restamp->process(data);

	data->setMediaTime(time);
	restamp = create<Transform::Restamp>(Transform::Restamp::Reset, 0);
	ConnectOutput(restamp.get(), onFrame);
	restamp->process(data);

	data->setMediaTime(time);
	restamp = create<Transform::Restamp>(Transform::Restamp::Reset, time);
	expected = time;
	ConnectOutput(restamp.get(), onFrame);
	restamp->process(data);
}

unittest("restamp: reset with offsets") {
	int64_t time = 10001;
	int64_t offset = -100;
	int64_t expected = time;
	auto onFrame = [&](Data data) {
		ASSERT(data->getMediaTime() == expected);
	};
	auto data = make_shared<DataRaw>(0);

	data->setMediaTime(time);
	auto restamp = create<Transform::Restamp>(Transform::Restamp::Passthru);
	ConnectOutput(restamp.get(), onFrame);
	restamp->process(data);

	data->setMediaTime(time);
	restamp = create<Transform::Restamp>(Transform::Restamp::Passthru, 0);
	ConnectOutput(restamp.get(), onFrame);
	restamp->process(data);

	data->setMediaTime(time);
	restamp = create<Transform::Restamp>(Transform::Restamp::Passthru, offset);
	expected = time + offset;
	ConnectOutput(restamp.get(), onFrame);
	restamp->process(data);

	data->setMediaTime(time);
	restamp = create<Transform::Restamp>(Transform::Restamp::Passthru, time);
	expected = time + time;
	ConnectOutput(restamp.get(), onFrame);
	restamp->process(data);
}
