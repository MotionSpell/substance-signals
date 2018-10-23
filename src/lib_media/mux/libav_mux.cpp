#include "libav_mux.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_modules/utils/helper_dyn.hpp"
#include "lib_modules/utils/factory.hpp"
#include "lib_utils/tools.hpp"
#include "../common/ffpp.hpp"
#include "../common/libav.hpp"
#include <cassert>
#include <string>
#include <sstream>

extern "C" {
#include <libavformat/avformat.h> // AVOutputFormat
#include <libavformat/avio.h> // avio_open2
}

using namespace Modules;

namespace {

class LibavMux : public ModuleDynI {
	public:

		LibavMux(IModuleHost* host, MuxConfig cfg)
			: m_host(host), m_formatCtx(avformat_alloc_context()), optionsDict(typeid(*this).name(), cfg.options) {
			if (!m_formatCtx)
				throw error("Format context couldn't be allocated.");
			m_formatCtx->flags &= ~AVFMT_FLAG_AUTO_BSF;

			auto const of = av_guess_format(cfg.format.c_str(), nullptr, nullptr);
			if (!of) {
				formatsList();
				throw error("Couldn't guess output format. Check list above for supported ones.");
			}
			m_formatCtx->oformat = of;

			std::stringstream fileName;
			fileName << cfg.baseName;
			std::stringstream formatExts(of->extensions); //get the first extension recommended by ffmpeg
			std::string fileNameExt;
			std::getline(formatExts, fileNameExt, ',');
			if (fileName.str().find("://") == std::string::npos) {
				fileName << "." << fileNameExt;
			}
			if (!(m_formatCtx->oformat->flags & AVFMT_NOFILE)) { /* open the output file, if needed */
				if (avio_open2(&m_formatCtx->pb, fileName.str().c_str(), AVIO_FLAG_READ_WRITE, nullptr, &optionsDict) < 0) {
					avformat_free_context(m_formatCtx);
					throw error(format("could not open %s, disable output.", cfg.baseName));
				}
			}
			strncpy(m_formatCtx->filename, fileName.str().c_str(), sizeof(m_formatCtx->filename) - 1);
			m_formatCtx->filename[(sizeof m_formatCtx->filename)-1] = 0;

			av_dict_set(&m_formatCtx->metadata, "service_provider", "GPAC Licensing Signals", 0);
			av_dump_format(m_formatCtx, 0, cfg.baseName.c_str(), 1);

			if (!cfg.format.compare(0, 5, "mpegts") || !cfg.format.compare(0, 3, "hls")) {
				m_inbandMetadata = true;
			}
		}

		~LibavMux() {
			if (m_formatCtx) {
				if (m_headerWritten) {
					av_write_trailer(m_formatCtx); //write the trailer if any
				}

				if (!(m_formatCtx->oformat->flags & AVFMT_NOFILE)) {
					avio_flush(m_formatCtx->pb);
					if (!(m_formatCtx->flags & AVFMT_FLAG_CUSTOM_IO)) {
						avio_close(m_formatCtx->pb); //close output file
					}
				}

				for (unsigned i = 0; i < m_formatCtx->nb_streams; ++i) {
					avcodec_close(m_formatCtx->streams[i]->codec);
				}

				avformat_free_context(m_formatCtx);
			}
		}

		void process() override {
			size_t inputIdx = 0;
			Data data;
			while (!inputs[inputIdx]->tryPop(data)) {
				inputIdx++;
			}
			auto prevInputMeta = inputs[inputIdx]->getMetadata();
			if (inputs[inputIdx]->updateMetadata(data)) {
				if (prevInputMeta) {
					m_host->log(*prevInputMeta == *inputs[inputIdx]->getMetadata() ? Debug : Error, format("Updating existing metadata on input port %s. Not supported but continuing execution.", inputIdx).c_str());
				} else if (!declareStream(data, inputIdx))
					return; //stream declared statically: no data to process.
			}
			if (m_formatCtx->nb_streams < (size_t)getNumInputs() - 1) {
				m_host->log(Warning, "Data loss due to undeclared streams on input ports. Consider declaring them statically.");
				return;
			}
			ensureHeader();

			auto pkt = getFormattedPkt(data);
			assert(pkt->pts != (int64_t)AV_NOPTS_VALUE);
			auto const pktTimescale = safe_cast<const MetadataPktLibav>(data->getMetadata())->getTimeScale();
			const AVRational inputTimebase = { (int)pktTimescale.den, (int)pktTimescale.num };
			auto const avStream = m_formatCtx->streams[inputIdx2AvStream[inputIdx]];
			pkt->dts = av_rescale_q(pkt->dts, inputTimebase, avStream->time_base);
			pkt->pts = av_rescale_q(pkt->pts, inputTimebase, avStream->time_base);
			pkt->duration = (int64_t)av_rescale_q(pkt->duration, inputTimebase, avStream->time_base);
			pkt->stream_index = avStream->index;

			if (av_interleaved_write_frame(m_formatCtx, pkt) != 0) {
				m_host->log(Warning, "can't write frame.");
				return;
			}
			av_packet_free(&pkt);
		}

	private:
		IModuleHost* const m_host;


		struct AVFormatContext *m_formatCtx;
		std::map<size_t, size_t> inputIdx2AvStream;
		ffpp::Dict optionsDict;
		bool m_headerWritten = false;
		bool m_inbandMetadata = false;

		bool declareStream(Data data, size_t inputIdx) {
			auto const metadata_ = data->getMetadata();
			auto metadata = std::dynamic_pointer_cast<const MetadataPktLibav>(metadata_);
			if(!metadata)
				throw error("Stream creation failed: unknown type.");

			AVStream *avStream = avformat_new_stream(m_formatCtx, metadata->getAVCodecContext()->codec);
			if (!avStream)
				throw error("Stream creation failed.");
			if (avcodec_parameters_from_context(avStream->codecpar, metadata->getAVCodecContext().get()) < 0)
				throw error("Stream parameters copy failed.");
			avStream->time_base = avStream->codec->time_base = metadata->getAVCodecContext()->time_base;
			inputIdx2AvStream[inputIdx] = m_formatCtx->nb_streams - 1;

			auto refData = std::dynamic_pointer_cast<const DataBaseRef>(data);
			return !(refData && !refData->getData());
		}

		void ensureHeader() {
			if (!m_headerWritten) {
				if (avformat_write_header(m_formatCtx, &optionsDict) != 0) {
					m_host->log(Warning, "fatal error: can't write the container header");
					for (unsigned i = 0; i < m_formatCtx->nb_streams; i++) {
						auto codec = m_formatCtx->streams[i]->codec;
						auto codecPar = m_formatCtx->streams[i]->codecpar;
						if (codec && codec->codec) {
							m_host->log(Debug, format("codec[%s] is \"%s\" (%s)", i, codec->codec->name, codec->codec->long_name).c_str());
							if (!codec->extradata) {
								if (codecPar->extradata) {
									codec->extradata = (uint8_t*)av_malloc(codec->extradata_size);
									codec->extradata_size = codecPar->extradata_size;
									memcpy(codec->extradata, codecPar->extradata, codecPar->extradata_size);
								} else
									throw error("Bitstream format is not raw. Check your encoder settings.");
							}
						}
					}
				} else {
					optionsDict.ensureAllOptionsConsumed();
					m_headerWritten = true;
				}
			}
		}

		AVPacket * getFormattedPkt(Data data) {
			auto pkt = safe_cast<const DataAVPacket>(data)->getPacket();
			auto videoMetadata = std::dynamic_pointer_cast<const MetadataPktLibavVideo>(data->getMetadata()); //video only ATM
			if (m_inbandMetadata && videoMetadata && (pkt->flags & AV_PKT_FLAG_KEY)) {
				auto const eSize = videoMetadata->codecSpecificInfo.size();
				auto const outSize = pkt->size + eSize;
				auto newPkt = av_packet_alloc();
				av_init_packet(newPkt);
				av_new_packet(newPkt, (int)outSize);
				memcpy(newPkt->data, videoMetadata->codecSpecificInfo.data(), eSize);
				memcpy(newPkt->data + eSize, pkt->data, pkt->size);
				newPkt->size = (int)outSize;
				newPkt->flags = pkt->flags;
				newPkt->dts = pkt->dts;
				newPkt->pts = pkt->pts;
				newPkt->duration = pkt->duration;
				return newPkt;
			} else {
				return av_packet_clone(pkt);
			}
		}

		void formatsList() {
			g_Log->log(Warning, "Output formats list:");
			AVOutputFormat *fmt = nullptr;
			while ((fmt = av_oformat_next(fmt))) {
				g_Log->log(Warning, format("fmt->name=%s, fmt->mime_type=%s, fmt->extensions=%s", fmt->name ? fmt->name : "", fmt->mime_type ? fmt->mime_type : "", fmt->extensions ? fmt->extensions : "").c_str());
			}
		}

};

Modules::IModule* createObject(IModuleHost* host, va_list va) {
	auto config = va_arg(va, MuxConfig*);
	enforce(host, "LibavMux: host can't be NULL");
	enforce(config, "LibavMux: config can't be NULL");

	avformat_network_init();

	return Modules::create<LibavMux>(host, *config).release();
}

auto const registered = Factory::registerModule("LibavMux", &createObject);
}
