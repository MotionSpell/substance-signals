
#include "tests.hpp"
#include "lib_media/decode/libav_decode.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/encode/libav_encode.hpp"
#include "lib_media/transform/audio_convert.hpp"
#include "lib_media/transform/restamp.hpp"
#include "lib_media/transform/video_convert.hpp"
#include "lib_media/utils/recorder.hpp"
#include <cmath>

using namespace Tests;
using namespace Modules;

template<typename T>
void checkTimestamps(const std::vector<int64_t> &timesIn, const std::vector<int64_t> &timesOut) {
	Encode::LibavEncode::Params p;
	p.frameRate.num = 1;
	std::shared_ptr<DataBase> picture = uptr(new PictureYUV420P(VIDEO_RESOLUTION));
	auto encode = create<Encode::LibavEncode>(Encode::LibavEncode::Video, p);
	auto mux = create<Mux::GPACMuxMP4>("random_ts");
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
	auto demux = create<T>("random_ts.mp4");
	Connect(demux->getOutput(0)->getSignal(), onFrame);
	demux->process(nullptr);
	ASSERT(i == timesOut.size());
}

unittest("timestamps start at random values (LibavDemux)") {
	const int64_t interval = (int64_t)Clock::Rate;
	const std::vector<int64_t> correct = { interval, 2 * interval, 3 * interval };
	const std::vector<int64_t> incorrect = { 0 };
	checkTimestamps<Demux::LibavDemux>(correct, incorrect);
}

unittest("timestamps start at random values (GPACDemuxMP4Simple)") {
	const int64_t interval = (int64_t)Clock::Rate;
	const std::vector<int64_t> correct = { interval, 2 * interval, 3 * interval };
	checkTimestamps<Demux::GPACDemuxMP4Simple>(correct, correct);
}

unittest("timestamps start at a negative value (LibavDemux)") {
	const int64_t interval = (int64_t)Clock::Rate;
	const std::vector<int64_t> correct = { -interval, 0, interval };
	const std::vector<int64_t> incorrect = { 0 };
	checkTimestamps<Demux::LibavDemux>(correct, incorrect);
}

unittest("timestamps start at a negative value (GPACDemuxMP4Simple)") {
	const int64_t interval = (int64_t)Clock::Rate;
	const std::vector<int64_t> correct = { -interval, 0, interval };
	checkTimestamps<Demux::GPACDemuxMP4Simple>(correct, correct);
}

unittest("transcoder with reframers: test a/v sync recovery") {
	const int64_t maxDurIn180k = 2 * Clock::Rate;
	auto const bufferSize = (maxDurIn180k * 1000) / (20 * Clock::Rate);

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
			libavAudioCtx2pcmConvert(safe_cast<const MetadataPktLibavAudio>(metadataDemux)->getAVCodecContext(), &demuxFmt);
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
			libavAudioCtx2pcmConvert(safe_cast<const MetadataPktLibavAudio>(metadataDemux)->getAVCodecContext(), &demuxFmt);
			auto const metaEnc = safe_cast<const MetadataPktLibavAudio>(metadataEncoder);
			auto format = PcmFormat(demuxFmt.sampleRate, demuxFmt.numChannels, demuxFmt.layout, encFmt.sampleFormat, (encFmt.numPlanes == 1) ? Interleaved : Planar);
			libavAudioCtx2pcmConvert(metaEnc->getAVCodecContext(), &encFmt);
			return create<Transform::AudioConvert>(format, metaEnc->getFrameSize());
		} else
			throw std::runtime_error("[Converter] Found unknown stream");
	};

	auto demux = create<Demux::LibavDemux>("data/beepbop.mp4");
	std::vector<std::unique_ptr<IModule>> modules;
	std::vector<std::unique_ptr<Utils::Recorder>> recorders;
	for (size_t i = 0; i < demux->getNumOutputs(); ++i) {
		auto const metadataDemux = safe_cast<const MetadataPktLibav>(demux->getOutput(i)->getMetadata());
		auto gapper = create<Gapper>();
		ConnectOutputToInput(demux->getOutput(i), gapper->getInput(0));
		auto decoder = create<Decode::LibavDecode>(metadataDemux);
		ConnectOutputToInput(gapper->getOutput(0), decoder->getInput(0));
		
		auto inputRes = metadataDemux->isVideo() ? safe_cast<const MetadataPktLibavVideo>(demux->getOutput(i)->getMetadata())->getResolution() : Resolution();
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
		Data data;
		int64_t lastMediaTime = 0;
		while ((data = recorders[g]->pop())) {
			Log::msg(Debug, "recv[%s] %s", g, data->getMediaTime());
			lastMediaTime = data->getMediaTime();
		}
		ASSERT(llabs(maxDurIn180k - lastMediaTime) < maxDurIn180k / 30);
	}
}

unittest("restamp: passthru with offsets") {
	auto const time = 10001LL;
	auto data = std::make_shared<DataRaw>(0);

	data->setMediaTime(time);
	auto restamp = create<Transform::Restamp>(Transform::Restamp::Reset);
	restamp->process(data);
	ASSERT_EQUALS(0, data->getMediaTime());

	data->setMediaTime(time);
	restamp = create<Transform::Restamp>(Transform::Restamp::Reset, 0);
	restamp->process(data);
	ASSERT_EQUALS(0, data->getMediaTime());

	data->setMediaTime(time);
	restamp = create<Transform::Restamp>(Transform::Restamp::Reset, time);
	restamp->process(data);
	ASSERT_EQUALS(time, data->getMediaTime());
}

unittest("restamp: reset with offsets") {
	int64_t time = 10001;
	int64_t offset = -100;
	auto data = std::make_shared<DataRaw>(0);

	data->setMediaTime(time);
	auto restamp = create<Transform::Restamp>(Transform::Restamp::Passthru);
	restamp->process(data);
	ASSERT_EQUALS(time, data->getMediaTime());

	data->setMediaTime(time);
	restamp = create<Transform::Restamp>(Transform::Restamp::Passthru, 0);
	restamp->process(data);
	ASSERT_EQUALS(time, data->getMediaTime());

	data->setMediaTime(time);
	restamp = create<Transform::Restamp>(Transform::Restamp::Passthru, offset);
	restamp->process(data);
	ASSERT_EQUALS(time + offset, data->getMediaTime());

	data->setMediaTime(time);
	restamp = create<Transform::Restamp>(Transform::Restamp::Passthru, time);
	restamp->process(data);
	ASSERT_EQUALS(time + time, data->getMediaTime());
}
