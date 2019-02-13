#include "mpegts_muxer.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_modules/utils/helper_dyn.hpp"
#include "lib_modules/utils/factory.hpp"
#include "lib_utils/tools.hpp"
#include "../../common/ffpp.hpp"
#include "../../common/libav.hpp" // avStrError
#include "../../common/metadata.hpp"
#include "../../common/attributes.hpp"
#include <cassert>
#include <string>

extern "C" {
#include <libavformat/avformat.h> // AVOutputFormat
#include <libavformat/avio.h> // avio_alloc_context
}

static auto const TS_PACKET_SIZE = 188;

using namespace Modules;
using namespace std;

namespace {

class TsMuxer : public ModuleDynI {
	public:

		TsMuxer(KHost* host, TsMuxerConfig cfg)
			: m_host(host), m_cfg(cfg), m_formatCtx(avformat_alloc_context())  {
			if (!m_formatCtx)
				throw error("Format context couldn't be allocated.");

			m_output = addOutput<OutputDefault>();

			m_formatCtx->oformat = av_guess_format("mpegts", nullptr, nullptr);
			enforce(m_formatCtx->oformat, "The 'mpegts' format must exist. Please check your ffmpeg build");
			enforce(!(m_formatCtx->oformat->flags & AVFMT_NOFILE), "Invalid mpegts format flags");

			m_formatCtx->pb = avio_alloc_context(m_outputBuffer, (int(sizeof m_outputBuffer)/TS_PACKET_SIZE)*TS_PACKET_SIZE, 1, this, nullptr, &staticOnWrite, nullptr);
			enforce(m_formatCtx->pb, "avio_alloc_context failed");
		}

		~TsMuxer() {
			{
				// HACK: we don't want to flush here, but it's the only way
				// to free the memory allocated by avformat_write_header.
				m_dropAllOutput = true; // prevent all output (calls to getBuffer/post)
				if (!m_flushed && m_headerWritten)
					av_write_trailer(m_formatCtx);
			}

			av_free(m_formatCtx->pb);
			avformat_free_context(m_formatCtx);
		}

		void process() override {

			// HACK: av_interleaved_write_frame will segfault if already flushed
			if(m_flushed) {
				m_host->log(Warning, "Ignoring input data after flush");
				return;
			}

			int inputIdx;
			auto data = popAny(inputIdx);
			auto prevInputMeta = inputs[inputIdx]->getMetadata();
			if (inputs[inputIdx]->updateMetadata(data)) {
				if (prevInputMeta) {
					if(!(*prevInputMeta == *inputs[inputIdx]->getMetadata()))
						m_host->log(Error, format("input #%s: updating existing metadata. Not supported but continuing execution.", inputIdx).c_str());
				} else {
					assert(!m_headerWritten);
					declareStream(data, inputIdx);
				}
			}

			if ((int)m_formatCtx->nb_streams < getNumInputs() - 1) {
				if(!m_dropping)
					m_host->log(Warning, "Some inputs didn't declare their streams yet, dropping input data");
				m_dropping = true;
				return;
			}

			if(m_dropping)
				m_host->log(Warning, "All streams declared: starting to mux");

			m_dropping = false;

			// if 'data' is a stream declaration, there's no actual data to process.
			if (isDeclaration(data))
				return;

			ensureHeader();

			AVPacket pkt;
			fillAvPacket(data, &pkt);
			const AVRational inputTimebase = { (int)1, (int)IClock::Rate };
			auto const avStream = m_formatCtx->streams[inputIdx2AvStream[inputIdx]];
			pkt.dts = av_rescale_q(pkt.dts, inputTimebase, avStream->time_base);
			pkt.pts = av_rescale_q(pkt.pts, inputTimebase, avStream->time_base);
			pkt.stream_index = avStream->index;

			int ret = av_interleaved_write_frame(m_formatCtx, &pkt);
			if (ret) {
				m_host->log(Warning, format("can't write frame: %s", avStrError(ret)).c_str());
				return;
			}
		}

		void flush() override {
			//write the trailer if any
			if (m_headerWritten)
				av_write_trailer(m_formatCtx);

			avio_flush(m_formatCtx->pb);

			m_flushed = true;
		}

	private:
		KHost* const m_host;
		TsMuxerConfig const m_cfg;
		AVFormatContext* const m_formatCtx;
		std::map<size_t, size_t> inputIdx2AvStream;
		bool m_headerWritten = false;
		bool m_flushed = false;
		bool m_dropping = false; // used for log message limitation
		bool m_dropAllOutput = false; // block all calls to getBuffer/post
		uint8_t m_outputBuffer[1024 * 1024];
		OutputDefault* m_output {};

		Data popAny(int& inputIdx) {
			Data data;
			inputIdx = 0;
			while (!inputs[inputIdx]->tryPop(data)) {
				inputIdx++;
			}
			return data;
		}

		void declareStream(Data data, size_t inputIdx) {
			if(!data->getMetadata())
				throw error("Can't declare stream without metadata");

			auto const metadata = safe_cast<const MetadataPkt>(data->getMetadata().get());

			enforce(metadata->bitrate >= 0, "bitrate must be specified for each ES");

			auto const codec = avcodec_find_decoder_by_name(metadata->codec.c_str());
			if (!codec)
				throw error(format("Codec not found: '%s'.", metadata->codec));

			auto stream = avformat_new_stream(m_formatCtx, codec);
			if (!stream)
				throw error("Stream creation failed.");

			auto codecpar = stream->codecpar;

			codecpar->codec_id = codec->id;

			if(auto info = dynamic_cast<const MetadataPktVideo*>(metadata)) {
				codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
				codecpar->width  = info->resolution.width;
				codecpar->height = info->resolution.height;
			} else if(auto info = dynamic_cast<const MetadataPktAudio*>(metadata)) {
				codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
				codecpar->sample_rate = info->sampleRate;
				codecpar->channels = info->numChannels;
				codecpar->frame_size = info->frameSize;
			} else {
				// anything to do for subtitles?
			}

			auto& extradata = metadata->codecSpecificInfo;
			codecpar->extradata_size = extradata.size();
			codecpar->extradata = (uint8_t*)av_malloc(extradata.size());
			if(extradata.size())
				memcpy(codecpar->extradata, extradata.data(), extradata.size());

			inputIdx2AvStream[inputIdx] = m_formatCtx->nb_streams - 1;
		}

		void ensureHeader() {
			if (m_headerWritten)
				return;
			AVDictionary* dict = nullptr;
			av_dict_set_int(&dict, "muxrate", m_cfg.muxRate, 0);
			av_dict_set_int(&dict, "max_delay", 5000 * 1000, 0); // Avoids "dts < pcr, TS is invalid" messages
			int ret = avformat_write_header(m_formatCtx, &dict);
			assert(!dict);
			if (ret != 0)
				throw error(format("can't write the container header: %s", avStrError(ret)));

			m_headerWritten = true;
		}

		void fillAvPacket(Data data, AVPacket* newPkt) {

			// only insert headers for video, not for audio (e.g would break AAC)
			auto const videoMetadata = dynamic_cast<const MetadataPktVideo*>(data->getMetadata().get());
			auto const key = data->get<CueFlags>().keyframe;
			auto const insertHeaders = videoMetadata && key;

			auto const& headers = insertHeaders ? videoMetadata->codecSpecificInfo : std::vector<uint8_t>();
			auto const outSize = data->data().len + headers.size();

			av_init_packet(newPkt);
			av_new_packet(newPkt, (int)outSize);

			if(key)
				newPkt->flags |= AV_PKT_FLAG_KEY;

			if(headers.size())
				memcpy(newPkt->data, headers.data(), headers.size());
			memcpy(newPkt->data + headers.size(), data->data().ptr, data->data().len);
			newPkt->size = (int)outSize;
			newPkt->pts = data->getMediaTime();
			newPkt->dts = data->get<DecodingTime>().time;

			// av_interleaved_write_frame will block if PTS/DTS don't start near zero
			if(m_mediaTimeOrigin == INT64_MIN)
				m_mediaTimeOrigin = -data->getMediaTime();

			newPkt->pts += m_mediaTimeOrigin;
			newPkt->dts += m_mediaTimeOrigin;
		}

		// workaround av_interleaved_write_frame blocking
		int64_t m_mediaTimeOrigin = INT64_MIN;

		// output handling: called by libavformat
		static int staticOnWrite(void* opaque, uint8_t* buf, int len) {
			auto pThis = (TsMuxer*)opaque;
			return pThis->onWrite({buf, (size_t)len});
		}

		int64_t m_sentBits = 0;

		int onWrite(SpanC packet) {
			assert(packet.len % TS_PACKET_SIZE == 0);

			if(!m_dropAllOutput) {
				while(packet.len > 0) {
					auto buf = m_output->getBuffer<DataRaw>(TS_PACKET_SIZE);
					memcpy(buf->data().ptr, packet.ptr, TS_PACKET_SIZE);
					buf->setMediaTime((m_sentBits * IClock::Rate) / m_cfg.muxRate);
					m_output->post(buf);
					packet += TS_PACKET_SIZE;
					m_sentBits += TS_PACKET_SIZE * 8;
				}
			}

			return packet.len;
		}
};

Modules::IModule* createObject(KHost* host, void* arg) {
	auto config = reinterpret_cast<TsMuxerConfig*>(arg);
	enforce(host, "TsMuxer: host can't be NULL");
	enforce(config, "TsMuxer: config can't be NULL");

	auto const BUFFER_SIZE = 2 * 1024 * 1024; // 2 Mb total
	return new ModuleDefault<TsMuxer>(BUFFER_SIZE/TS_PACKET_SIZE, host, *config);
}

auto const registered = Factory::registerModule("TsMuxer", &createObject);
}
