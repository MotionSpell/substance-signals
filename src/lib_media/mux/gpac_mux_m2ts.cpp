#include "gpac_mux_m2ts.hpp"
#include "../common/gpacpp.hpp"
#include "../common/libav.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_modules/utils/helper_dyn.hpp"
#include "lib_modules/utils/factory.hpp"
#include "lib_utils/log_sink.hpp" // Debug
#include <cassert>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <gpac/mpegts.h>
#include <gpac/esi.h> // GF_ESInterface
}

using namespace Modules;

namespace {

// wrapper for GF_M2TS_Mux
class M2TSMux : public gpacpp::Init {
	public:
		M2TSMux(bool real_time, unsigned mux_rate, unsigned pcr_ms = 100, int64_t pcr_init_val = -1) {
			const GF_M2TS_PackMode single_au_pes = GF_M2TS_PACK_AUDIO_ONLY;
			const int pcrOffset = 0;
			const int curPid = 100;

			muxer = gf_m2ts_mux_new(mux_rate, GF_M2TS_PSI_DEFAULT_REFRESH_RATE, real_time == true ? GF_TRUE : GF_FALSE);
			if (!muxer)
				throw std::runtime_error("[GPACMuxMPEG2TS] Could not create the muxer.");

			gf_m2ts_mux_use_single_au_pes_mode(muxer, single_au_pes);
			if (pcr_init_val >= 0)
				gf_m2ts_mux_set_initial_pcr(muxer, (u64)pcr_init_val);

			gf_m2ts_mux_set_pcr_max_interval(muxer, pcr_ms);
			program = gf_m2ts_mux_program_add(muxer, 1, curPid, GF_M2TS_PSI_DEFAULT_REFRESH_RATE, pcrOffset, GF_FALSE, 0, GF_TRUE);
			if (!program)
				throw std::runtime_error("[GPACMuxMPEG2TS] Could not create the muxer.");
		}

		~M2TSMux() {
			gf_m2ts_mux_del(muxer);
		}

		void addStream(std::unique_ptr<GF_ESInterface> ifce, u32 PID, Bool isPCR) {
			auto stream = gf_m2ts_program_stream_add(program, ifce.get(), PID, isPCR, GF_FALSE);
			if (ifce->stream_type == GF_STREAM_VISUAL) stream->start_pes_at_rap = GF_TRUE;
			ifces.push_back(std::move(ifce));
		}

		/*return nullptr or a 188 byte packet*/
		const char* process(u32 &status, u32 &usec_till_next) {
			return gf_m2ts_mux_process(muxer, &status, &usec_till_next);
		}

	private:
		GF_M2TS_Mux *muxer = nullptr;
		GF_M2TS_Mux_Program *program = nullptr;
		std::vector<std::unique_ptr<GF_ESInterface>> ifces;
};

typedef Queue<AVPacket*> DataInput;

class GPACMuxMPEG2TS : public ModuleDynI, public gpacpp::Init {
	public:
		GPACMuxMPEG2TS(IModuleHost* host, TsMuxConfig* cfg);
		~GPACMuxMPEG2TS();
		void process() override;

	private:
		IModuleHost* const m_host;

		void declareStream(Data data);
		GF_Err fillInput(GF_ESInterface *esi, u32 ctrl_type, size_t inputIdx);
		static GF_Err staticFillInput(GF_ESInterface *esi, u32 ctrl_type, void *param);

		std::unique_ptr<M2TSMux> muxer;
		std::vector<std::unique_ptr<DataInput>> inputData;
		OutputDefault *output;
};

struct UserData {
	UserData(GPACMuxMPEG2TS *muxer, size_t inputIdx) : muxer(muxer), inputIdx(inputIdx) {}
	GPACMuxMPEG2TS *muxer; //TODO: constify?
	size_t inputIdx; //TODO: constify?
};

GPACMuxMPEG2TS::GPACMuxMPEG2TS(IModuleHost* host, TsMuxConfig* cfg)
	:  m_host(host), muxer(new M2TSMux(cfg->real_time, cfg->mux_rate, cfg->pcr_ms, cfg->pcr_init_val)) {
	output = addOutput<OutputDefault>();
}

GPACMuxMPEG2TS::~GPACMuxMPEG2TS() {
	for (int i = 0; i < getNumInputs() - 1; ++i) {
		AVPacket *data = nullptr;
		while (inputData[i]->tryPop(data)) {
			av_packet_free(&data);
		}
	}

#if 0
	for (j = 0; j<sources[i].nb_streams; j++)
		if (sources[i].streams[j].input_ctrl) sources[i].streams[j].input_ctrl(&sources[i].streams[j], GF_ESI_INPUT_DESTROY, NULL);
#endif
}

GF_Err GPACMuxMPEG2TS::staticFillInput(GF_ESInterface *esi, u32 ctrl_type, void * /*param*/) {
	auto userData = (UserData*)esi->input_udta;
	return userData->muxer->fillInput(esi, ctrl_type, userData->inputIdx);
}

GF_Err GPACMuxMPEG2TS::fillInput(GF_ESInterface *esi, u32 ctrl_type, size_t inputIdx) {
	switch (ctrl_type) {
	case GF_ESI_INPUT_DATA_FLUSH: {
		AVPacket *pkt = nullptr;
		while (inputData[inputIdx-1]->tryPop(pkt)) {
			GF_ESIPacket pck;
			memset(&pck, 0, sizeof(GF_ESIPacket));

			pck.flags = GF_ESI_DATA_AU_START | GF_ESI_DATA_HAS_CTS | GF_ESI_DATA_HAS_DTS;
			if (pkt->flags & AV_PKT_FLAG_KEY) {
				pck.flags |= GF_ESI_DATA_AU_RAP;
			}

			pck.dts = pkt->dts;
			pck.cts = pkt->pts;

#if 0
			if (priv->sample->IsRAP && priv->dsi && priv->dsi_size) {
				pck.data = (char*)priv->dsi;
				pck.data_len = priv->dsi_size;
				ifce->output_ctrl(ifce, GF_ESI_OUTPUT_DATA_DISPATCH, &pck);
				pck.flags &= ~GF_ESI_DATA_AU_START;
			}
#endif

			pck.flags |= GF_ESI_DATA_AU_END;
			pck.data = (char*)pkt->data;
			pck.data_len = pkt->size;
			pck.duration = (u32)pkt->duration;

			esi->output_ctrl(esi, GF_ESI_OUTPUT_DATA_DISPATCH, &pck);
			m_host->log(Debug, format("Dispatch CTS %s\n", pck.cts).c_str());

			av_packet_free(&pkt);
		}
		break;
	}
	case GF_ESI_INPUT_DATA_RELEASE:
		return GF_OK;
	case GF_ESI_INPUT_DESTROY:
		delete (UserData*)esi->input_udta;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}

	return GF_OK;
}

void GPACMuxMPEG2TS::declareStream(Data data) {
	const size_t inputIdx = inputs.size() - 1;
	inputData.push_back(make_unique<DataInput>());

	auto ifce = make_unique<GF_ESInterface>();
	memset(ifce.get(), 0, sizeof(GF_ESInterface));

	auto const metadata_ = data->getMetadata();
	if (auto metadata = std::dynamic_pointer_cast<const MetadataPktLibavVideo>(metadata_)) {
		addInput(new Input(this));

		ifce->caps = GF_ESI_SIGNAL_DTS;
		ifce->stream_id = 1;
		ifce->program_number = 0;
		ifce->stream_type = GF_STREAM_VISUAL;
		ifce->object_type_indication = metadata->codec == "h264" ? GPAC_OTI_VIDEO_AVC : GPAC_OTI_VIDEO_HEVC;
		ifce->fourcc = 0;
		ifce->lang = 0;
		ifce->timescale = (u32)metadata->timeScale.num;
		assert(metadata->timeScale.den == 1);
		ifce->duration = 0;
		ifce->bit_rate = 0;
		ifce->repeat_rate = 0;
		(void)ifce->decoder_config;
		(void)ifce->decoder_config_size;
#if 0
	} else if (auto metadata2 = std::dynamic_pointer_cast<const MetadataPktLibavAudio>(metadata_)) {
#if 0 //TODO
		auto input = addInput(new Input(this));
		throw std::runtime_error("[GPACMuxMPEG2TS] Stream (audio) creation failed: unknown type.");
#endif
		ifce.info_audio.sample_rate = 0;
		ifce.info_audio.nb_channels = 0;
#endif
	} else
		throw std::runtime_error("[GPACMuxMPEG2TS] Stream creation failed: unknown type.");

	//TODO: Fill the interface with test content; the current GPAC importer needs to be generalized
	//ifce->caps |= GF_ESI_AU_PULL_CAP;
	ifce->input_ctrl = &GPACMuxMPEG2TS::staticFillInput;
	ifce->input_udta = new UserData(this, inputIdx);
	ifce->output_udta = nullptr;

	Bool isPCR = GF_TRUE; //FIXME: crash if no PCR (ifce.stream_type == GF_STREAM_AUDIO) ? GF_TRUE : GF_FALSE;
	const int curPid = 100; //FIXME: duplicate
	muxer->addStream(std::move(ifce), (u32)(curPid + inputIdx), isPCR);
}

void GPACMuxMPEG2TS::process() {
	for (int i = 0; i < getNumInputs() - 1; ++i) {
		Data data;
		while (inputs[i]->tryPop(data)) {
			if (inputs[i]->updateMetadata(data))
				declareStream(data);

			auto encoderData = safe_cast<const DataAVPacket>(data);
			auto dst = av_packet_alloc();
			av_packet_ref(dst, encoderData->getPacket());
			inputData[i]->push(dst);
		}
	}

	const char *ts_pck;
	u32 status, usec_till_next;
	while ((ts_pck = muxer->process(status, usec_till_next))) {
		auto data = output->getBuffer(188);
		memcpy(data->data().ptr, ts_pck, 188);
		getOutput(0)->emit(data);
	}
}

Modules::IModule* createObject(IModuleHost* host, va_list va) {
	auto config = va_arg(va, TsMuxConfig*);
	enforce(host, "GPACMuxMPEG2TS: host can't be NULL");
	enforce(config, "GPACMuxMPEG2TS: config can't be NULL");
	return Modules::create<GPACMuxMPEG2TS>(host, config).release();
}

auto const registered = Factory::registerModule("GPACMuxMPEG2TS", &createObject);
}
