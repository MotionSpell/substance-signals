#include "gpac_filter_mem_out.h"
#include <string.h>

#define OFFS(_n) #_n, offsetof(MemOutCtx, _n)

const GF_FilterArgs MemOutArgs[] = {
	{ OFFS(dst), "destination", GF_PROP_NAME, NULL, NULL, 0 },
	{ NULL, 0, NULL, GF_PROP_FORBIDEN, NULL, NULL, GF_FS_ARG_HINT_NORMAL }
};

static const GF_FilterCapability MemOutCaps[] = {
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG4),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_LCP),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_SSRP),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG2_PART3),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG_AUDIO),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_AC3),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_EAC3),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG2_MAIN),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_AV1),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_VP8),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_VP9),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_VP10),
};

static GF_Err mem_out_initialize(GF_Filter *filter) {
	MemOutCtx* ctx = (MemOutCtx*)gf_filter_get_udta(filter);
	if (!ctx)
		return GF_BAD_PARAM;

	ctx->last_dts = ctx->last_pts = ctx->last_inc = GF_FILTER_NO_TS;

	return GF_OK;
}

static GF_FilterProbeScore mem_out_probe_url(const char *url, const char *mime_type) {
	(void)(mime_type);
	if (!strncmp(url, "signals://", 10))
		return GF_FPROBE_SUPPORTED;
	else
		return GF_FPROBE_NOT_SUPPORTED;
}

static Bool mem_out_process_event(GF_Filter *filter, const GF_FilterEvent *evt) {
	MemOutCtx* ctx = (MemOutCtx*) gf_filter_get_udta(filter);

	if (evt->base.on_pid && (evt->base.on_pid != ctx->pid))
		return GF_FALSE;

	switch (evt->base.type) {
	case GF_FEVT_STOP:
		gf_filter_pid_set_eos(ctx->pid);
		ctx->eos = GF_TRUE;
		return GF_TRUE;
	default:
		break;
	}

	return GF_FALSE;
}

static GF_Err mem_out_process(GF_Filter *filter) {
	GF_FilterPacket *pck = NULL;
	const GF_PropertyValue *prop = NULL;
	MemOutCtx* ctx = (MemOutCtx*)gf_filter_get_udta(filter);

	if (ctx->eos)
		return GF_EOS;

	prop = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_DECODER_CONFIG);
	if (prop && prop->value.data.ptr && prop->value.data.size) {
		ctx->pushDsi(ctx->parent, prop->value.data.ptr, prop->value.data.size);
	}
	pck = gf_filter_pid_get_packet(ctx->pid);
	if (pck) {
		u32 data_size = 0;
		u64 dts = gf_filter_pck_get_dts(pck);
		u64 pts = gf_filter_pck_get_cts(pck);
		const u8 *data = gf_filter_pck_get_data(pck, &data_size);

		// fix audio reframing inconsistencies
		prop = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_CODECID);
		if (prop && gf_codecid_type(prop->value.uint) == GF_STREAM_AUDIO) {
			if (ctx->last_dts == GF_FILTER_NO_TS) { // init
				ctx->last_dts = dts;
			} else if (ctx->last_inc == GF_FILTER_NO_TS) { // compute last_inc
				ctx->last_inc = dts - ctx->last_dts;
				ctx->last_dts = dts;
			} else if (dts == ctx->last_dts + ctx->last_inc) { // normal case
				ctx->last_dts = dts;
			} else if (ctx->last_dts == dts) { // repeated: infer
				ctx->last_dts = ctx->last_dts + ctx->last_inc;
				ctx->last_inc = GF_FILTER_NO_TS;
			} else { // discontinuity
				ctx->last_dts = dts;
				ctx->last_inc = GF_FILTER_NO_TS;
			}

			ctx->last_pts = pts;
		} else {
			ctx->last_dts = dts;
			ctx->last_pts = pts;
		}

		ctx->pushData(ctx->parent, data, data_size, ctx->last_dts, ctx->last_pts);

		gf_filter_pid_drop_packet(ctx->pid);
	}

	return GF_OK;
}

GF_Err mem_out_configure_pid(GF_Filter *filter, GF_FilterPid *PID, Bool is_remove) {
	(void)(is_remove);
	MemOutCtx* ctx = (MemOutCtx*)gf_filter_get_udta(filter);

	if (!ctx->pid) {
		GF_FilterEvent evt;
		GF_FEVT_INIT(evt, GF_FEVT_PLAY, PID);
		gf_filter_pid_send_event(PID, &evt);
	}

	ctx->pid = PID;
	return GF_OK;
}

GF_FilterRegister memOutRegister = {
	.name = "mem_out",
	GF_FS_SET_DESCRIPTION("Memory output")
	GF_FS_SET_HELP("\n")
	.private_size = sizeof(MemOutCtx),
	.args = MemOutArgs,
	.initialize = mem_out_initialize,
	SETCAPS(MemOutCaps),
	.process = mem_out_process,
	.process_event = mem_out_process_event,
	.probe_url = mem_out_probe_url,
	.configure_pid = mem_out_configure_pid,
};

const GF_FilterRegister *mem_out_register(GF_FilterSession *session) {
	(void)(session);
	return &memOutRegister;
}
