#include "libav_demux.hpp"
#include "../transform/restamp.hpp"
#include "lib_utils/tools.hpp"
#include "lib_utils/os.hpp"
#include "lib_ffpp/ffpp.hpp"
#include <cassert>
#include <fstream>

#define PKT_QUEUE_SIZE 256

namespace Modules {

namespace {

constexpr
const char* webcamFormat() {
#ifdef _WIN32
	return "dshow";
#elif __linux__
	return "v4l2";
#elif __APPLE__
	return "avfoundation";
#else
#error "unknown platform"
#endif
}

}

namespace Demux {

static const int avioCtxBufferSize = 1024 * 1024;

void LibavDemux::webcamList() {
	log(Warning, "Webcam list:");
	ffpp::Dict dict(typeid(*this).name(), "-list_devices true");
	avformat_open_input(&m_formatCtx, "video=dummy:audio=dummy", av_find_input_format(webcamFormat()), &dict);
	log(Warning, "Webcam example: webcam:video=\"Integrated Webcam\":audio=\"Microphone (Realtek High Defini\"");
}

bool LibavDemux::webcamOpen(const std::string &options) {
	auto avInputFormat = av_find_input_format(webcamFormat());
	if (avformat_open_input(&m_formatCtx, options.c_str(), avInputFormat, nullptr)) {
		avformat_free_context(m_formatCtx);
		m_formatCtx = nullptr;
		return false;
	}
	return true;
}

void LibavDemux::initRestamp() {
	restampers.resize(m_formatCtx->nb_streams);
	for (unsigned i = 0; i < m_formatCtx->nb_streams; i++) {
		const std::string format(m_formatCtx->iformat->name);
		const std::string fn = m_formatCtx->filename;
		if (format == "rtsp" || format == "rtp" || format == "sdp" || !fn.compare(0, 4, "rtp:") || !fn.compare(0, 4, "udp:")) {
			restampers[i] = create<Transform::Restamp>(Transform::Restamp::IgnoreFirstAndReset);
		} else {
			restampers[i] = create<Transform::Restamp>(Transform::Restamp::Reset);
		}

		if (format == "rtsp" || format == "rtp" || format == "mpegts") {
			auto stream = m_formatCtx->streams[i];
			startPTSIn180k = std::max<int64_t>(startPTSIn180k, timescaleToClock(stream->start_time*stream->time_base.num, stream->time_base.den));
		} else {
			startPTSIn180k = 0;
		}
	}
}

LibavDemux::LibavDemux(const std::string &url, const bool loop, const std::string &avformatCustom, const uint64_t seekTimeInMs, const std::string &formatName, LibavDemux::ReadFunc avioCustom)
	: loop(loop), done(false), dispatchPkts(PKT_QUEUE_SIZE), m_read(std::move(avioCustom)) {
	if (!(m_formatCtx = avformat_alloc_context()))
		throw error("Can't allocate format context");

	const std::string device = url.substr(0, url.find(":"));
	if (device == "webcam") {
		if (url == device || !webcamOpen(url.substr(url.find(":") + 1))) {
			webcamList();
			clean();
			throw error("Webcam init failed.");
		}

		restampers.resize(m_formatCtx->nb_streams);
		for (unsigned i = 0; i < m_formatCtx->nb_streams; i++) {
			restampers[i] = create<Transform::Restamp>(Transform::Restamp::ClockSystem); /*some webcams timestamps don't start at 0 (based on UTC)*/
		}
	} else {
		ffpp::Dict dict(typeid(*this).name(), "-buffer_size 1M -fifo_size 1M -probesize 10M -analyzeduration 10M -overrun_nonfatal 1 -protocol_whitelist file,udp,rtp,http,https,tcp,tls,rtmp -rtsp_flags prefer_tcp -correct_ts_overflow 1 " + avformatCustom);

		if (m_read) {
			m_avioCtx = avio_alloc_context((unsigned char*)av_malloc(avioCtxBufferSize), avioCtxBufferSize, 0, this, &LibavDemux::read, nullptr, nullptr);
			if (!m_avioCtx)
				throw std::runtime_error("AvIO allocation failed");
			m_formatCtx->pb = m_avioCtx;
			m_formatCtx->flags = AVFMT_FLAG_CUSTOM_IO;
		}
		if (avformat_open_input(&m_formatCtx, url.c_str(), av_find_input_format(formatName.c_str()), &dict)) {
			clean();
			throw error(format("Error when opening input '%s'", url));
		}
		m_formatCtx->flags |= AVFMT_FLAG_KEEP_SIDE_DATA; //deprecated >= 3.5 https://github.com/FFmpeg/FFmpeg/commit/ca2b779423

		if (seekTimeInMs) {
			if (avformat_seek_file(m_formatCtx, -1, INT64_MIN, convertToTimescale(seekTimeInMs, 1000, AV_TIME_BASE), INT64_MAX, 0) < 0) {
				clean();
				throw error(format("Couldn't seek to time %sms", seekTimeInMs));
			}

			log(Info, "Successful initial seek to %sms", seekTimeInMs);
		}

		//if you don't call you may miss the first frames
		if (avformat_find_stream_info(m_formatCtx, nullptr) < 0) {
			clean();
			throw error("Couldn't get additional video stream info");
		}

		initRestamp();

		av_dict_free(&dict);
	}

	lastDTS.resize(m_formatCtx->nb_streams);
	offsetIn180k.resize(m_formatCtx->nb_streams);
	for (unsigned i = 0; i<m_formatCtx->nb_streams; i++) {
		auto const st = m_formatCtx->streams[i];
		auto const parser = av_stream_get_parser(st);
		if (parser) {
			st->codec->ticks_per_frame = parser->repeat_pict + 1;
		} else {
			log(Debug, format("No parser found for stream %s (%s). Couldn't use full metadata to get the timescale.", i, avcodec_get_name(st->codec->codec_id)));
		}
		st->codec->time_base = st->time_base; //allows to keep trace of the pkt timebase in the output metadata
		if (!st->codec->framerate.num) {
			st->codec->framerate = st->avg_frame_rate; //it is our reponsibility to provide the application with a reference framerate
		}

		std::shared_ptr<IMetadata> m;
		auto codecCtx = shptr(avcodec_alloc_context3(nullptr));
		avcodec_copy_context(codecCtx.get(), st->codec);
		switch (st->codec->codec_type) {
		case AVMEDIA_TYPE_AUDIO: m = shptr(new MetadataPktLibavAudio(codecCtx, st->id)); break;
		case AVMEDIA_TYPE_VIDEO: m = shptr(new MetadataPktLibavVideo(codecCtx, st->id)); break;
		case AVMEDIA_TYPE_SUBTITLE: m = shptr(new MetadataPktLibavSubtitle(codecCtx, st->id)); break;
		default: break;
		}
		outputs.push_back(addOutput<OutputDataDefault<DataAVPacket>>(m));
		av_dump_format(m_formatCtx, i, url.c_str(), 0);
	}
}

void LibavDemux::clean() {
	done = true;
	if (workingThread.joinable()) {
		workingThread.join();
	}

	if (m_formatCtx) {
		avformat_close_input(&m_formatCtx);
		avformat_free_context(m_formatCtx);
	}

	AVPacket p;
	while (dispatchPkts.read(p)) {
		av_free_packet(&p);
	}

	if(m_avioCtx)
		av_free(m_avioCtx->buffer);
	av_free(m_avioCtx);
}

LibavDemux::~LibavDemux() {
	clean();
}

void LibavDemux::seekToStart() {
	if (av_seek_frame(m_formatCtx, -1, m_formatCtx->start_time, AVSEEK_FLAG_ANY) < 0)
		throw error(format("Couldn't seek to start time %s", m_formatCtx->start_time));

	for (unsigned i = 0; i < m_formatCtx->nb_streams; i++) {
		offsetIn180k[i] += timescaleToClock(m_formatCtx->duration, AV_TIME_BASE);
	}
}

bool LibavDemux::rectifyTimestamps(AVPacket &pkt) {
	auto const stream = m_formatCtx->streams[pkt.stream_index];
	auto const maxDelayInSec = 5;
	auto const thresholdInBase = (maxDelayInSec * stream->time_base.den) / stream->time_base.num;

	if (pkt.dts != AV_NOPTS_VALUE) {
		pkt.dts += clockToTimescale(offsetIn180k[pkt.stream_index] * stream->time_base.num, stream->time_base.den);
		if (lastDTS[pkt.stream_index] && pkt.dts < lastDTS[pkt.stream_index]
		    && (1LL << stream->pts_wrap_bits) - lastDTS[pkt.stream_index] < thresholdInBase && pkt.dts + (1LL << stream->pts_wrap_bits) > lastDTS[pkt.stream_index]) {
			offsetIn180k[pkt.stream_index] += timescaleToClock((1LL << stream->pts_wrap_bits) * stream->time_base.num, stream->time_base.den);
			log(Warning, "Stream %s: overflow detecting on DTS (%s, last=%s, timescale=%s/%s, offset=%s).",
			    pkt.stream_index, pkt.dts, lastDTS[pkt.stream_index], stream->time_base.num, stream->time_base.den, clockToTimescale(offsetIn180k[pkt.stream_index] * stream->time_base.num, stream->time_base.den));
		}
	}

	if (pkt.pts != AV_NOPTS_VALUE) {
		pkt.pts += clockToTimescale(offsetIn180k[pkt.stream_index] * stream->time_base.num, stream->time_base.den);
		if (pkt.pts < pkt.dts && (1LL << stream->pts_wrap_bits) + pkt.pts - pkt.dts < thresholdInBase) {
			auto const localOffsetIn180k = timescaleToClock((1LL << stream->pts_wrap_bits) * stream->time_base.num, stream->time_base.den);
			log(Warning, "Stream %s: overflow detecting on PTS (%s, new=%s, timescale=%s/%s, offset=%s).",
			    pkt.stream_index, pkt.pts, pkt.pts + localOffsetIn180k, stream->time_base.num, stream->time_base.den, clockToTimescale(offsetIn180k[pkt.stream_index] * stream->time_base.num, stream->time_base.den));
			pkt.pts += localOffsetIn180k;
		}
	}

	if (pkt.pts != AV_NOPTS_VALUE && pkt.dts != AV_NOPTS_VALUE && pkt.pts < pkt.dts) {
		log(Error, "Stream %s: pts < dts (%s < %s)", pkt.stream_index, pkt.pts, pkt.dts);
		return false;
	}

	return true;
}

void LibavDemux::threadProc() {

	if(!setHighThreadPriority())
		log(Warning, "Couldn't change reception thread priority to realtime.");

	AVPacket pkt;
	bool nextPacketResetFlag = false;
	while (!done) {
		av_init_packet(&pkt);
		int status = av_read_frame(m_formatCtx, &pkt);
		if (status < 0 || !rectifyTimestamps(pkt)) {
			av_free_packet(&pkt);

			if (status == (int)AVERROR(EAGAIN)) {
				log(Debug, "Stream asks to try again later. Sleeping for a short period of time.");
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}

			if (status == (int)AVERROR_EOF || (m_formatCtx->pb && m_formatCtx->pb->eof_reached)) {
				log(Info, "End of stream detected - %s", loop ? "looping" : "leaving");
				if (loop) {
					seekToStart();
					nextPacketResetFlag = true;
					continue;
				}
			} else if (m_formatCtx->pb && m_formatCtx->pb->error) {
				log(Error, "Stream contains an irrecoverable error (%s) - leaving", status);
			}
			done = true;
			return;
		}

		if (nextPacketResetFlag) {
			pkt.flags |= AV_PKT_FLAG_RESET_DECODER;
			nextPacketResetFlag = false;
		}

		while (!dispatchPkts.write(pkt)) {
			if (done) {
				av_free_packet(&pkt);
				return;
			}
			log(m_formatCtx->pb && !m_formatCtx->pb->seekable ? Warning : Debug, "Dispatch queue is full - regulating.");
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
}

void LibavDemux::setMediaTime(std::shared_ptr<DataAVPacket> data) {
	auto pkt = data->getPacket();
	if (!pkt->duration) {
		pkt->duration = pkt->dts - lastDTS[pkt->stream_index];
	}
	lastDTS[pkt->stream_index] = pkt->dts;
	auto const base = m_formatCtx->streams[pkt->stream_index]->time_base;
	auto const time = timescaleToClock(pkt->dts * base.num, base.den);
	data->setMediaTime(time - startPTSIn180k);
	int64_t offset;
	if (startPTSIn180k) {
		offset = -startPTSIn180k; //a global offset is applied to all streams (since it is a PTS we may have negative DTSs)
	} else {
		data->setMediaTime(restampers[pkt->stream_index]->restamp(data->getMediaTime())); //restamp by pid only when no start time
		offset = data->getMediaTime() - time;
	}
	if (offset != 0) {
		data->restamp(offset * base.num, base.den); //propagate to AVPacket
	}
}

bool LibavDemux::dispatchable(AVPacket * const pkt) {
	if (pkt->flags & AV_PKT_FLAG_CORRUPT) {
		log(Error, "Corrupted packet received (DTS=%s).", pkt->dts);
	}
	if (pkt->dts == AV_NOPTS_VALUE) {
		pkt->dts = lastDTS[pkt->stream_index];
		log(Debug, "No DTS: setting last value %s.", pkt->dts);
	}
	if (!lastDTS[pkt->stream_index]) {
		auto stream = m_formatCtx->streams[pkt->stream_index];
		auto minPts = clockToTimescale(startPTSIn180k*stream->time_base.num, stream->time_base.den);
		if(pkt->pts < minPts) {
			av_free_packet(pkt);
			return false;
		}
	}
	return true;
}

void LibavDemux::dispatch(AVPacket *pkt) {
	auto out = outputs[pkt->stream_index]->getBuffer(0);
	auto outPkt = out->getPacket();
	av_packet_move_ref(outPkt, pkt);
	setMediaTime(out);
	outputs[outPkt->stream_index]->emit(out);
	sparseStreamsHeartbeat(outPkt);
}

void LibavDemux::sparseStreamsHeartbeat(AVPacket const * const pkt) {
	int64_t curDTS = 0;

	// signal clock from audio to sparse streams
	// (should be the PCR but libavformat doesn't give access to it)
	if (m_formatCtx->streams[pkt->stream_index]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
		auto const base = m_formatCtx->streams[pkt->stream_index]->time_base;
		auto const time = timescaleToClock(pkt->dts * base.num, base.den);
		if (time > curDTS) {
			curDTS = time;
		}
	}
	if (curDTS > curTimeIn180k) {
		curTimeIn180k = curDTS;
		for (unsigned i = 0; i < m_formatCtx->nb_streams; ++i) {
			if ((int)i == pkt->stream_index) {
				continue;
			}
			auto const st = m_formatCtx->streams[i];
			if (st->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
				auto outParse = outputs[i]->getBuffer(0);
				outParse->setMediaTime(curTimeIn180k);
				outputs[i]->emit(outParse);
			}
		}
	}
}

void LibavDemux::process(Data data) {
	workingThread = std::thread(&LibavDemux::threadProc, this);

	AVPacket pkt;
	while (1) {
		if (getNumInputs() && getInput(0)->tryPop(data)) {
			done = true;
			log(Info, "Exit from an external event.");
			return;
		}

		if (!dispatchPkts.read(pkt)) {
			if (done) {
				log(Info, "All data consumed: exit process().");
				return;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}
		if (dispatchable(&pkt)) {
			dispatch(&pkt);
		}
	}

	while (dispatchPkts.read(pkt)) {
		dispatch(&pkt);
	}
}

}
}
