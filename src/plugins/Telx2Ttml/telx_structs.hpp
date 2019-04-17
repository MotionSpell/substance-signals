#pragma once

#include "telx_tables.hpp"
#include "lib_utils/log.hpp"
#include "lib_utils/format.hpp"

namespace {

typedef enum { //awkward type required by the spec states
	Undef = 255,
	No = 0,
	Yes = 1,
} Bool;

typedef struct {
	uint8_t current;
	uint8_t G0_M29;
	uint8_t G0_X28;
} PrimaryCharset;

typedef struct {
	Bool progInfoProcessed;
} State;

// entities, used in color mode, to replace unsafe HTML tag chars
typedef struct {
	uint16_t character;
	const char *entity;
} Entity;
Entity const entities[] = {
	{ '<', "&lt;" },
	{ '>', "&gt;" },
	{ '&', "&amp;" }
};

typedef enum {
	NonSubtitle = 0x02,
	Subtitle = 0x03,
	Inverted = 0x0c,
	DataUnitVPS = 0xc3,
	DataUnitClosedCaptions = 0xc5
} DataUnit;

typedef struct {
	uint8_t unused_clock_in;
	uint8_t unused_framing_code;
	uint8_t address[2];
	uint8_t data[40];
} Payload;

uint8_t unham_8_4(uint8_t a) {
	uint8_t val = UnHam_8_4[a];
	if (val == 0xff) {
		val = 0;
		g_Log->log(Warning, format("Teletext: unrecoverable data error (4): %s\n", a).c_str());
	}
	return (val & 0x0f);
}

#define MAGAZINE(telx_page) ((telx_page >> 8) & 0xf)
#define PAGE(telx_page) (telx_page & 0xff)

uint32_t unham_24_18(uint32_t a) {
	// Section 8.3
	uint8_t test = 0;
	for (uint8_t i = 0; i < 23; i++) {
		test ^= ((a >> i) & 0x01) * (i + 33);
	}
	test ^= ((a >> 23) & 0x01) * 32;

	if ((test & 0x1f) != 0x1f) {
		if ((test & 0x20) == 0x20) {
			return 0xffffffff;
		}
		a ^= 1 << (30 - test);
	}

	return (a & 0x000004) >> 2 | (a & 0x000070) >> 3 | (a & 0x007f00) >> 4 | (a & 0x7f0000) >> 5;
}

void ucs2_to_utf8(char *out, uint16_t in) {
	if (in < 0x80) {
		out[0] = in & 0x7f;
		out[1] = 0;
		out[2] = 0;
		out[3] = 0;
	} else if (in < 0x800) {
		out[0] = (in >> 6) | 0xc0;
		out[1] = (in & 0x3f) | 0x80;
		out[2] = 0;
		out[3] = 0;
	} else {
		out[0] = (in >> 12) | 0xe0;
		out[1] = ((in >> 6) & 0x3f) | 0x80;
		out[2] = (in & 0x3f) | 0x80;
		out[3] = 0;
	}
}

typedef enum {
	Parallel = 0,
	Serial = 1
} TransmissionMode;

typedef struct {
	uint64_t showTimestamp;
	uint64_t hideTimestamp;
	uint16_t text[25][40];
	uint8_t tainted; // 1 = text variable contains any data
} PageBuffer;

}
