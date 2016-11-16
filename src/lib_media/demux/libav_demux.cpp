#include "libav_demux.hpp"
#include "../transform/restamp.hpp"
#include "../common/libav.hpp"
#include "lib_utils/tools.hpp"
#include "lib_ffpp/ffpp.hpp"
#include <cassert>
#include <fstream>

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
}

namespace Modules {

namespace {

//#define MPEGTS_PCR_DISPATCH //TODO: add mpegts only (format == "mpegts") checks

auto g_InitAv = runAtStartup(&av_register_all);
auto g_InitAvformat = runAtStartup(&avformat_network_init);
auto g_InitAvcodec = runAtStartup(&avcodec_register_all);
auto g_InitAvdevice = runAtStartup(&avdevice_register_all);
auto g_InitAvLog = runAtStartup(&av_log_set_callback, avLog);

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

bool isRaw(AVCodecContext *codecCtx) {
	return codecCtx->codec_id == AV_CODEC_ID_RAWVIDEO;
}

#ifdef MPEGTS_PCR_DISPATCH
//FIXME: this is a huge hack to get the PCR. Without PCR, synchronization is broken.
extern "C" {
#define NB_PID_MAX 8192
struct MpegTSFilter {
	int pid;
	int es_id;
	int last_cc; /* last cc code (-1 if first packet) */
	int64_t last_pcr;
};
struct MpegTSContext {
	void *c;
	/* user data */
	AVFormatContext *stream;
	/** raw packet size, including FEC if present */
	int raw_packet_size;

	int size_stat[3];
	int size_stat_count;
#define SIZE_STAT_THRESHOLD 10

	int64_t pos47_full;

	/** if true, all pids are analyzed to find streams */
	int auto_guess;

	/** compute exact PCR for each transport stream packet */
	int mpeg2ts_compute_pcr;

	/** fix dvb teletext pts                                 */
	int fix_teletext_pts;

	int64_t cur_pcr;    /**< used to estimate the exact PCR */
	int pcr_incr;       /**< used to estimate the exact PCR */

    /* data needed to handle file based ts */
    /** stop parsing loop */
	int stop_parse;
	/** packet containing Audio/Video data */
	AVPacket *pkt;
	/** to detect seek */
	int64_t last_pos;

	int skip_changes;
	int skip_clear;

	int scan_all_pmts;

	int resync_size;

	/******************************************/
	/* private mpegts data */
	/* scan context */
	/** structure to keep track of Program->pids mapping */
	unsigned int nb_prg;
	struct Program *prg;

	int8_t crc_validity[NB_PID_MAX];
	/** filters for various streams specified by PMT + for the PAT and PMT */
	MpegTSFilter *pids[NB_PID_MAX];
	int current_pid;
};
}
#endif

}

namespace Demux {
void LibavDemux::webcamList() {
	log(Warning, "Webcam list:");
	ffpp::Dict dict(typeid(*this).name(), "format", "-list_devices true");
	avformat_open_input(&m_formatCtx, "video=dummy:audio=dummy", av_find_input_format(webcamFormat()), &dict);
	log(Warning, "Webcam example: webcam:video=\"Integrated Webcam\":audio=\"Microphone (Realtek High Defini\"");
}

bool LibavDemux::webcamOpen(const std::string &options) {
	auto avInputFormat = av_find_input_format(webcamFormat());
	if (avformat_open_input(&m_formatCtx, options.c_str(), avInputFormat, nullptr))
		return false;
	return true;
}

LibavDemux::LibavDemux(const std::string &url, const uint64_t seekTimeInMs) {
	if (!(m_formatCtx = avformat_alloc_context()))
		throw error("Can't allocate format context");

	const std::string device = url.substr(0, url.find(":"));
	if (device == "webcam") {
		if (url == device || !webcamOpen(url.substr(url.find(":") + 1))) {
			webcamList();
			if (m_formatCtx) avformat_close_input(&m_formatCtx);
			throw error("Webcam init failed.");
		}

		restampers.resize(m_formatCtx->nb_streams);
		for (unsigned i = 0; i < m_formatCtx->nb_streams; i++) {
			restampers[i] = uptr(create<Transform::Restamp>(Transform::Restamp::ClockSystem)); /*some webcams timestamps don't start at 0 (based on UTC)*/
		}
	} else {
		ffpp::Dict dict(typeid(*this).name(), "demuxer", "-fifo_size 1000000 -probesize 100M -analyzeduration 100M -overrun_nonfatal 1 -protocol_whitelist file,udp,rtp,http,https,tcp,tls,rtmp -rtsp_flags prefer_tcp");
		if (avformat_open_input(&m_formatCtx, url.c_str(), nullptr, &dict)) {
			if (m_formatCtx) avformat_close_input(&m_formatCtx);
			throw error(format("Error when opening input '%s'", url));
		}

		if (seekTimeInMs) {
			if(avformat_seek_file(m_formatCtx, -1, INT64_MIN, (seekTimeInMs * AV_TIME_BASE) / 1000, INT64_MAX, 0) < 0) {
				avformat_close_input(&m_formatCtx);
				throw error(format("Couldn't seek to time %sms", seekTimeInMs));
			} else {
				log(Info, "Successful initial seek to %sms", seekTimeInMs);
			}
		}

		//if you don't call you may miss the first frames
		m_formatCtx->max_analyze_duration = 0;
		if (avformat_find_stream_info(m_formatCtx, nullptr) < 0) {
			avformat_close_input(&m_formatCtx);
			throw error("Couldn't get additional video stream info");
		}

		restampers.resize(m_formatCtx->nb_streams);
		dispatchPkts.resize(m_formatCtx->nb_streams);
		for (unsigned i = 0; i < m_formatCtx->nb_streams; i++) {
			const std::string format(m_formatCtx->iformat->name);
			const std::string  fn = m_formatCtx->filename;
			if (format == "rtsp" || format == "rtp" || format == "sdp" || !fn.compare(0, 4,  "rtp:") || !fn.compare(0, 4, "udp:")) {
				restampers[i] = uptr(create<Transform::Restamp>(Transform::Restamp::IgnoreFirstTimestamp));
			} else {
				restampers[i] = uptr(create<Transform::Restamp>(Transform::Restamp::Reset));
			}

			if (format == "mpegts") {
				startPTS = std::max<int64_t>(startPTS, m_formatCtx->streams[i]->start_time);
			}
		}

		av_dict_free(&dict);
	}

	for (unsigned i = 0; i<m_formatCtx->nb_streams; i++) {
		AVStream *st = m_formatCtx->streams[i];
		auto parser = av_stream_get_parser(st);
		if (parser) {
			st->codec->ticks_per_frame = parser->repeat_pict + 1;
		} else {
			log(Info, format("No parser found for stream %s (%s). Couldn't use full metadata to get the timescale.", i, st->codec->codec_name));
		}

		IMetadata *m;
		switch (st->codec->codec_type) {
		case AVMEDIA_TYPE_AUDIO: m = new MetadataPktLibavAudio(st->codec, st->id); break;
		case AVMEDIA_TYPE_VIDEO: /*ffpp::isRaw(st->codec) ? m = new MetadataRawVideo :*/
			m = new MetadataPktLibavVideo(st->codec, st->id); break;
		default: m = nullptr; break; //TODO:  sparse stream: send regularly empty samples
		}
		outputs.push_back(addOutput<OutputDataDefault<DataAVPacket>>(m));
		av_dump_format(m_formatCtx, i, "", 0);
	}
}

LibavDemux::~LibavDemux() {
	avformat_close_input(&m_formatCtx);
}

void LibavDemux::setTime(std::shared_ptr<DataAVPacket> data, int streamIdx) {
	auto pkt = data->getPacket();
	auto const base = m_formatCtx->streams[pkt->stream_index]->time_base;
	auto const time = timescaleToClock(pkt->dts * base.num, base.den);
	data->setTime(time);

	restampers[streamIdx]->process(data); /*restamp by pid only when no start time*/
	int64_t offset = data->getTime() - time;
	if (offset != 0) {
		data->restamp(offset * base.num, base.den); /*propagate to AVPacket*/
	}
}

void LibavDemux::dispatch() {
#ifdef MPEGTS_PCR_DISPATCH
	//TODO: add mpegts only (format == "mpegts") checks
	auto const PCR = ((MpegTSContext*)m_formatCtx->priv_data)->pids[m_formatCtx->programs[0]->pcr_pid]->last_pcr;
	if (PCR == -1) {
		return;
	}
#endif

	for (size_t i = 0; i < m_formatCtx->nb_streams; ++i) {
		auto p = dispatchPkts[i].begin();
		while (p != dispatchPkts[i].end()) {
			auto pkt = (*p)->getPacket();
			if (pkt->pts < startPTS) {
				p = dispatchPkts[i].erase(p);
				continue;
			}
#ifdef MPEGTS_PCR_DISPATCH
			if (PCR / 300 < pkt->pts) {
				break;
			} else
#endif
			{
				auto out = outputs[pkt->stream_index]->getBuffer(0);
				AVPacket *outPkt = out->getPacket();
				av_packet_move_ref(outPkt, pkt);
				setTime(out, outPkt->stream_index);
				outputs[outPkt->stream_index]->emit(out);
				p = dispatchPkts[i].erase(p);
			}
		}
	}
}

void LibavDemux::process(Data data) {
	for (;;) {
		if (getNumInputs() && getInput(0)->tryPop(data))
			break;

		auto p = uptr(new DataAVPacket);
		auto pkt = p->getPacket();
		int status = av_read_frame(m_formatCtx, pkt);
		if (status < 0) {
			if (status == (int)AVERROR_EOF || (m_formatCtx->pb && m_formatCtx->pb->eof_reached)) {
				log(Info, "End of stream detected - leaving");
			} else if (m_formatCtx->pb && m_formatCtx->pb->error) {
				log(Error, "Stream contains an irrecoverable error (%s) - leaving", status);
			}
			return;
		}

		dispatchPkts[pkt->stream_index].push_back(std::move(p));
		dispatch();

		//TODO: av_dict_set(&st->metadata, "language", language, 0);
	}

	log(Info, "Exit from an external event.");
}

}
}
