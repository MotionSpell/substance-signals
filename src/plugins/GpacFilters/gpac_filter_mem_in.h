#pragma once

#include <gpac/filters.h>

const GF_FilterRegister *mem_in_register(GF_FilterSession*);

typedef struct {
	char *src;
	GF_FilterPid *pid; //only one output pid declared
	Bool eos;

    // Signals parent object
    void *parent;
    void (*getData)(void *parent, const u8 **data, u32 *data_size, u64 *dts, u64 *pts);
    void (*freeData)(void *parent);
    const char *signals_codec_name;
} MemInCtx;
