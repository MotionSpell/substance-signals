#pragma once

#include <gpac/filters.h>

const GF_FilterRegister *mem_out_register(GF_FilterSession*);

typedef struct {
	char *dst;

	//only one output pid declared
	GF_FilterPid *pid;

	Bool eos;

    // Signals parent object
    void *parent;
    void (*pushData)(void *parent, const u8 *data, u32 data_size, u64 dts, u64 pts);
    void (*pushDsi)(void *parent, const u8 *data, u32 data_size);
} MemOutCtx;
