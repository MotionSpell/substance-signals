#include "gpac_filter_mem_in.h"

//Romain: test to compile as C99, C11, C++, ...
//https://stackoverflow.com/questions/855021/how-do-i-access-internal-members-of-a-union#855039

#define OFFS(_n)	#_n, offsetof(MemInCtx, _n)

const GF_FilterArgs MemInArgs[] = {
	{ OFFS(src), "source", GF_PROP_NAME, NULL, NULL, 0 },
	{ NULL, 0, NULL, GF_PROP_FORBIDEN, NULL, NULL, GF_FS_ARG_HINT_NORMAL }
};

static GF_Err mem_in_initialize(GF_Filter *filter) {
	MemInCtx* ctx = (MemInCtx*)gf_filter_get_udta(filter);
	if (!ctx)
		return GF_BAD_PARAM;

	return GF_OK;
}

//Romain:
//static void mem_in_finalize(GF_Filter */*filter*/) {
//}

static GF_FilterProbeScore mem_in_probe_url(const char *url, const char *mime_type) {
	(void)(mime_type);
	if (!strnicmp(url, "signals://", 10))
		return GF_FPROBE_SUPPORTED;
	else
		return GF_FPROBE_NOT_SUPPORTED;
}

static Bool mem_in_process_event(GF_Filter *filter, const GF_FilterEvent *evt) {
	MemInCtx* ctx = (MemInCtx*) gf_filter_get_udta(filter);

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

static void mem_in_pck_destructor(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck) {
	(void)(pid);
	(void)(pck);
	MemInCtx *ctx = (MemInCtx*)gf_filter_get_udta(filter);
	ctx->freeData(ctx->parent);
}

static GF_Err mem_in_process(GF_Filter *filter) {
	GF_Err e = GF_OK;
	const u8 *data = NULL;
	u32 data_size = 0;
	u64 dts = 0;
	MemInCtx* ctx = (MemInCtx*)gf_filter_get_udta(filter);

	if (ctx->eos)
		return GF_EOS;

	ctx->getData(ctx->parent, &data, &data_size, &dts);
	if (data) {
		if (!ctx->pid) {
			//e = gf_filter_pid_raw_new(filter, ctx->src, NULL, "audio/aac"/*Romain*/, NULL, data, data_size, GF_TRUE, &ctx->pid);
			ctx->pid = gf_filter_pid_new(filter);
			if (!ctx->pid) goto exit; //OUT OF MEMORY
			//if (e) goto exit;

			//Romain: setting the timescale creates a deadlock because there seems to be some buffering based on media times in gpac
			e = gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(180000));
			if (e) goto exit;

			e = gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_AUDIO));
			if (e) goto exit;

			e = gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_AAC_MPEG4));
			if (e) goto exit;

			e = gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE));
			if (e) goto exit;
		}

		GF_FilterPacket *pck = gf_filter_pck_new_shared(ctx->pid, data, data_size, mem_in_pck_destructor);
		if (!pck) {
			e = GF_OUT_OF_MEM;
			goto exit;
		}

		e = gf_filter_pck_set_dts(pck, dts);
		if (e) goto exit;

		e = gf_filter_pck_set_cts(pck, dts); //Romain: we should set the PTS
		if (e) goto exit;

		e = gf_filter_pck_send(pck);
		if (e) goto exit;
	}

exit:
	return e;
}

const GF_FilterCapability MemInCaps[] = {
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO/*FILE*/),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG4),
	/*CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_LCP),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_SSRP),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG2_PART3),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG1),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG_AUDIO),*/
};

GF_FilterRegister memInRegister = {
	.name = "mem_in",
	GF_FS_SET_DESCRIPTION("Memory input")
	GF_FS_SET_HELP("\n")
	.private_size = sizeof(MemInCtx),
	.args = MemInArgs,
	.initialize = mem_in_initialize,
	SETCAPS(MemInCaps),
	.process = mem_in_process,
	.process_event = mem_in_process_event,
	.probe_url = mem_in_probe_url
};

const GF_FilterRegister *mem_in_register(GF_FilterSession *session) {
	(void)(session);
	return &memInRegister;
}
