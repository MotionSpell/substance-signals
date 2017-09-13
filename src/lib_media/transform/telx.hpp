#include <cstdint>
#include <cstdio>
#include <memory>
#include "telx_tables.hpp"
#include "lib_utils/log.hpp"

//this code was written really fast to cover the teletext to ttml conversion
//sticks to the spec
//in this file we extract pages to convert them later
//should be moved in an external lib

namespace {

typedef enum { //awkward type required by the spec states
	UNDEF = 255,
	NO = 0,
	YES = 1,
} Bool;

typedef struct {
	uint8_t current;
	uint8_t G0_M29;
	uint8_t G0_X28;
} PrimaryCharset;
PrimaryCharset primaryCharset = { 0x00, UNDEF, UNDEF };

typedef struct {
	Bool progInfoProcessed;
	Bool PTSIsInit;
} State;
State states = { NO, NO };

// entities, used in color mode, to replace unsafe HTML tag chars
typedef struct {
	uint16_t character;
	char *entity;
} Entity;
Entity const entities[] = {
	{ '<', "&lt;" },
	{ '>', "&gt;" },
	{ '&', "&amp;" }
};

uint32_t framesProduced = 0;

typedef enum {
	NONSUBTITLE = 0x02,
	SUBTITLE = 0x03,
	INVERTED = 0x0c,
	DATA_UNIT_VPS = 0xc3,
	DATA_UNIT_CLOSED_CAPTIONS = 0xc5
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
		Log::msg(Warning, "Teletext: unrecoverable data error (4): %s\n", a);
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

uint8_t cc_map[256] = { 0 };

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

uint16_t telx_to_ucs2(uint8_t c) {
	if (Parity8[c] == 0) {
		Log::msg(Warning, "Teletext: Unrecoverable data error (5): %s\n", c);
		return 0x20;
	}

	uint16_t val = c & 0x7f;
	if (val >= 0x20) {
		val = G0[LATIN][val - 0x20];
	}
	return val;
}

typedef enum {
	PARALLEL = 0,
	SERIAL = 1
} TransmissionMode;
TransmissionMode transmissionMode = SERIAL;

uint8_t receivingData = NO; // flag indicating if incoming data should be processed or ignored

typedef struct {
	uint8_t verbose;
	uint16_t page;
	uint16_t tid;     // 13 bits packet ID for teletext stream
	uint8_t colors;   // output <font...></font> tags
	uint8_t bom;
	uint8_t nonempty; // produce at least one (dummy) frame
	uint64_t UTCReferenceTime;
	uint8_t seMode;
} Config;

Config config = {
	NO, 0, 0, NO, YES, NO, 0, NO,
};

typedef struct {
	uint64_t showTimestampInMs;
	uint64_t hideTimestampInMs;
	uint16_t text[25][40];
	uint8_t tainted; // 1 = text variable contains any data
} PageBuffer;
PageBuffer pageBuffer;

void remap_g0_charset(uint8_t c) {
	if (c != primaryCharset.current) {
		uint8_t m = G0_LATIN_NATIONAL_SUBSETS_MAP[c];
		if (m == 0xff) {
			Log::msg(Warning, "Teletext: G0 subset %s.%s is not implemented\n", (c >> 3), (c & 0x7));
		} else {
			for (uint8_t j = 0; j < 13; j++) {
				G0[LATIN][G0_LATIN_NATIONAL_SUBSETS_POSITIONS[j]] = G0_LATIN_NATIONAL_SUBSETS[m].characters[j];
			}
			primaryCharset.current = c;
		}
	}
}

std::unique_ptr<Modules::Transform::Page> process_page(PageBuffer *pageIn) {
	auto pageOut = uptr(new Modules::Transform::Page);
	uint8_t emptyPage = YES;
	for (uint8_t col = 0; col < 40; col++) {
		for (uint8_t row = 1; row < 25; row++) {
			if (pageIn->text[row][col] == 0x0b) {
				emptyPage = NO;
				goto emptyPage;
			}
		}
	}

emptyPage:
	if (emptyPage == YES)
		return pageOut;

	if (pageIn->showTimestampInMs > pageIn->hideTimestampInMs)
		pageIn->hideTimestampInMs = pageIn->showTimestampInMs;

	if (config.seMode == YES) {
		++framesProduced;
		pageOut->tsInMs = pageIn->showTimestampInMs;
	} else {
		pageOut->showTimestampInMs = pageIn->showTimestampInMs;
		pageOut->hideTimestampInMs = pageIn->hideTimestampInMs;
	}

	for (uint8_t row = 1; row < 25; row++) {
		uint8_t colStart = 40, colStop = 40;
		for (int8_t col = 39; col >= 0; col--) {
			if (pageIn->text[row][col] == 0xb) {
				colStart = col;
				break;
			}
		}
		if (colStart > 39)
			continue; //empty line

		for (uint8_t col = colStart + 1; col <= 39; col++) {
			if (pageIn->text[row][col] > 0x20) {
				if (colStop > 39) colStart = col;
				colStop = col;
			}
			if (pageIn->text[row][col] == 0xa)
				break;
		}
		if (colStop > 39)
			continue; //empty line

		// section 12.2: Alpha White ("Set-After") - Start-of-row default condition.
		uint8_t fgColor = 0x7; //white(7)
		uint8_t fontTagOpened = NO;
		for (uint8_t col = 0; col <= colStop; col++) {
			uint16_t val = pageIn->text[row][col];
			if (col < colStart) {
				if (val <= 0x7)
					fgColor = (uint8_t)val;
			}
			if (col == colStart) {
				if ((fgColor != 0x7) && (config.colors == YES)) {
					//TODO: look for "//colors:": fprintf(fout, "<font color=\"%s\">", TELX_COLORS[fgColor]);
					fontTagOpened = YES;
				}
			}

			if (col >= colStart) {
				if (val <= 0x7) {
					if (config.colors == YES) {
						if (fontTagOpened == YES) {
							//colors: fprintf(fout, "</font> ");
							fontTagOpened = NO;
						}
						if ((val > 0x0) && (val < 0x7)) {
							//colors: fprintf(fout, "<font color=\"%s\">", TELX_COLORS[v]);
							fontTagOpened = YES;
						}
					} else {
						val = 0x20;
					}
				}

				if (val >= 0x20) {
					if (config.colors == YES) {
						for (uint8_t i = 0; i < sizeof(entities) / sizeof(entities[0]); i++) {
							if (val == entities[i].character) { // translate chars into entities when in color mode
								//colors: fprintf(fout, "%s", entities[i].entity);
								val = 0; // v < 0x20 won't be printed in next block
								break;
							}
						}
					}
				}

				if (val >= 0x20) {
					char u[4] = { 0, 0, 0, 0 };
					ucs2_to_utf8(u, val);
					pageOut->ss << u;
				}
			}
		}

		if ((config.colors == YES) && (fontTagOpened == YES)) {
			//colors: fprintf(fout, "</font>");
			fontTagOpened = NO;
		}

		pageOut->ss << ((config.seMode == YES) ? " " : "\r\n"); // line delimiter
	}

	pageOut->ss << "\r\n";
	pageOut->ss.flush();
	return pageOut;
}

std::unique_ptr<Modules::Transform::Page> process_telx_packet(DataUnit dataUnitId, Payload *packet, uint64_t timestamp) {
	// section 7.1.2
	uint8_t address = (unham_8_4(packet->address[1]) << 4) | unham_8_4(packet->address[0]);
	uint8_t m = address & 0x7;
	if (m == 0)
		m = 8;
	uint8_t y = (address >> 3) & 0x1f;
	uint8_t designationCode = (y > 25) ? unham_8_4(packet->data[0]) : 0x00;
	std::unique_ptr<Modules::Transform::Page> pageOut;

	if (y == 0) {
		uint8_t i = (unham_8_4(packet->data[1]) << 4) | unham_8_4(packet->data[0]);
		uint8_t subtitleFlag = (unham_8_4(packet->data[5]) & 0x08) >> 3;
		cc_map[i] |= subtitleFlag << (m - 1);

		if ((config.page == 0) && (subtitleFlag == YES) && (i < 0xff)) {
			config.page = (m << 8) | (unham_8_4(packet->data[1]) << 4) | unham_8_4(packet->data[0]);
		}

		uint16_t pageNum = (m << 8) | (unham_8_4(packet->data[1]) << 4) | unham_8_4(packet->data[0]);
		uint8_t charset = ((unham_8_4(packet->data[7]) & 0x08) | (unham_8_4(packet->data[7]) & 0x04) | (unham_8_4(packet->data[7]) & 0x02)) >> 1;
		
		// Section 9.3.1.3
		transmissionMode = (TransmissionMode)(unham_8_4(packet->data[7]) & 0x01);
		if ((transmissionMode == PARALLEL) && (dataUnitId != SUBTITLE))
			return nullptr;

		if ((receivingData == YES) && (
			((transmissionMode == SERIAL) && (PAGE(pageNum) != PAGE(config.page))) ||
			((transmissionMode == PARALLEL) && (PAGE(pageNum) != PAGE(config.page)) && (m == MAGAZINE(config.page)))
			)) {
			receivingData = NO;
			return nullptr;
		}

		if (pageNum != config.page)
			return nullptr; //page transmission is terminated, however now we are waiting for our new page

		if (pageBuffer.tainted == YES) { //begining of page transmission
			pageBuffer.hideTimestampInMs = timestamp - 40;
			pageOut = process_page(&pageBuffer);
		}

		pageBuffer.showTimestampInMs = timestamp;
		pageBuffer.hideTimestampInMs = 0;
		memset(pageBuffer.text, 0x00, sizeof(pageBuffer.text));
		pageBuffer.tainted = NO;
		receivingData = YES;
		primaryCharset.G0_X28 = UNDEF;

		uint8_t c = (primaryCharset.G0_M29 != UNDEF) ? primaryCharset.G0_M29 : charset;
		remap_g0_charset(c);
	} else if ((m == MAGAZINE(config.page)) && (y >= 1) && (y <= 23) && (receivingData == YES)) {
		// Section 9.4.1
		for (uint8_t i = 0; i < 40; i++) {
			if (pageBuffer.text[y][i] == 0x00)
				pageBuffer.text[y][i] = telx_to_ucs2(packet->data[i]);
		}
		pageBuffer.tainted = YES;
	} else if ((m == MAGAZINE(config.page)) && (y == 26) && (receivingData == YES)) {
		// Section 12.3.2
		uint8_t X26Row = 0, X26Col = 0;
		uint32_t triplets[13] = { 0 };
		for (uint8_t i = 1, j = 0; i < 40; i += 3, j++) {
			triplets[j] = unham_24_18((packet->data[i + 2] << 16) | (packet->data[i + 1] << 8) | packet->data[i]);
		}

		for (uint8_t j = 0; j < 13; j++) {
			if (triplets[j] == 0xffffffff) {
				Log::msg(Warning, "Teletext: unrecoverable data error (1): %s\n", triplets[j]);
				continue;
			}

			uint8_t data = (triplets[j] & 0x3f800) >> 11;
			uint8_t mode = (triplets[j] & 0x7c0) >> 6;
			uint8_t address = triplets[j] & 0x3f;
			uint8_t row_address_group = (address >= 40) && (address <= 63);
			if ((mode == 0x04) && (row_address_group == YES)) {
				X26Row = address - 40;
				if (X26Row == 0) X26Row = 24;
				X26Col = 0;
			}
			if ((mode >= 0x11) && (mode <= 0x1f) && (row_address_group == YES))
				break; //termination marker

			if ((mode == 0x0f) && (row_address_group == NO)) {
				X26Col = address;
				if (data > 31) pageBuffer.text[X26Row][X26Col] = G2[0][data - 0x20];
			}

			if ((mode >= 0x11) && (mode <= 0x1f) && (row_address_group == NO)) {
				X26Col = address;
				if ((data >= 65) && (data <= 90)) { // A - Z
					pageBuffer.text[X26Row][X26Col] = G2_ACCENTS[mode - 0x11][data - 65];
				} else if ((data >= 97) && (data <= 122)) { // a - z
					pageBuffer.text[X26Row][X26Col] = G2_ACCENTS[mode - 0x11][data - 71];
				} else {
					pageBuffer.text[X26Row][X26Col] = telx_to_ucs2(data);
				}
			}
		}
	} else if ((m == MAGAZINE(config.page)) && (y == 28) && (receivingData == YES)) {
		// Section 9.4.7
		if ((designationCode == 0) || (designationCode == 4)) {
			uint32_t triplet0 = unham_24_18((packet->data[3] << 16) | (packet->data[2] << 8) | packet->data[1]);
			if (triplet0 == 0xffffffff) {
				Log::msg(Warning, "Teletext: unrecoverable data error (2): %s\n", triplet0);
			} else {
				if ((triplet0 & 0x0f) == 0x00) {
					primaryCharset.G0_X28 = (triplet0 & 0x3f80) >> 7;
					remap_g0_charset(primaryCharset.G0_X28);
				}
			}
		}
	} else if ((m == MAGAZINE(config.page)) && (y == 29)) {
		// Section 9.5.1
		if ((designationCode == 0) || (designationCode == 4)) {
			uint32_t triplet0 = unham_24_18((packet->data[3] << 16) | (packet->data[2] << 8) | packet->data[1]);
			if (triplet0 == 0xffffffff) {
				Log::msg(Warning, "Teletext: unrecoverable data error (3): %s\n", triplet0);
			} else {
				if ((triplet0 & 0xff) == 0x00) {
					primaryCharset.G0_M29 = (triplet0 & 0x3f80) >> 7;
					if (primaryCharset.G0_X28 == UNDEF) {
						remap_g0_charset(primaryCharset.G0_M29);
					}
				}
			}
		}
	} else if ((m == 8) && (y == 30)) {
		// Section 9.8
		if (states.progInfoProcessed == NO) {
			if (unham_8_4(packet->data[0]) < 2) {
				for (uint8_t i = 20; i < 40; i++) {
					uint8_t c = (uint8_t)telx_to_ucs2(packet->data[i]);
					if (c < 0x20)
						continue;

					char u[4] = { 0, 0, 0, 0 };
					ucs2_to_utf8(u, c);
				}

				//time conversion here is insane:
				// BCD to Modified Julian Day
				uint32_t t = 0;
				t += (packet->data[10] & 0x0f) * 10000;
				t += ((packet->data[11] & 0xf0) >> 4) * 1000;
				t += (packet->data[11] & 0x0f) * 100;
				t += ((packet->data[12] & 0xf0) >> 4) * 10;
				t += (packet->data[12] & 0x0f);
				t -= 11111;
				// conversion Modified Julian Day to unix timestamp
				t = (t - 40587) * 86400;
				// add time
				t += 3600 * (((packet->data[13] & 0xf0) >> 4) * 10 + (packet->data[13] & 0x0f));
				t += 60 * (((packet->data[14] & 0xf0) >> 4) * 10 + (packet->data[14] & 0x0f));
				t += (((packet->data[15] & 0xf0) >> 4) * 10 + (packet->data[15] & 0x0f));
				t -= 40271;

				time_t t0 = (time_t)t;
				if (config.seMode == YES) {
					config.UTCReferenceTime = t;
					states.PTSIsInit = NO;
				}
				states.progInfoProcessed = YES;
			}
		}
	}

	return pageOut;
}

}
