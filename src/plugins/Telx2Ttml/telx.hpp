#pragma once

#include <cstdint>
#include <memory>
#include "lib_utils/format.hpp"
#include "telx_tables.hpp"
#include "telx_structs.hpp"
#include "telx2ttml.hpp"

//this code was written really fast to cover the teletext to ttml conversion
//sticks to the spec
//in this file we extract pages to convert them later
//should be moved in an external lib

namespace {

struct TeletextState {
	Modules::KHost* host;
	uint16_t page = 0;
	uint8_t colors = No;   // output <font...></font> tags
	uint8_t seMode = No;
	PrimaryCharset primaryCharset = { 0x00, Undef, Undef };
	State states = { No };
	uint32_t framesProduced = 0;
	uint8_t cc_map[256] = { 0 };
	TransmissionMode transmissionMode = Serial;
	uint8_t receivingData = No; // flag indicating if incoming data should be processed or ignored
	PageBuffer pageBuffer;
	uint16_t G0[5][96] = { //G0 charsets in UCS-2
		{
			// Latin G0 Primary Set
			0x0020, 0x0021, 0x0022, 0x00a3, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
			0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
			0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
			0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005a, 0x00ab, 0x00bd, 0x00bb, 0x005e, 0x0023,
			0x002d, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
			0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x00bc, 0x00a6, 0x00be, 0x00f7, 0x007f
		},
		{
			// Cyrillic G0 Primary Set - Option 1 - Serbian/Croatian
			0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x044b, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
			0x0030, 0x0031, 0x3200, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
			0x0427, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413, 0x0425, 0x0418, 0x0408, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e,
			0x041f, 0x040c, 0x0420, 0x0421, 0x0422, 0x0423, 0x0412, 0x0403, 0x0409, 0x040a, 0x0417, 0x040b, 0x0416, 0x0402, 0x0428, 0x040f,
			0x0447, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433, 0x0445, 0x0438, 0x0428, 0x043a, 0x043b, 0x043c, 0x043d, 0x043e,
			0x043f, 0x042c, 0x0440, 0x0441, 0x0442, 0x0443, 0x0432, 0x0423, 0x0429, 0x042a, 0x0437, 0x042b, 0x0436, 0x0422, 0x0448, 0x042f
		},
		{
			// Cyrillic G0 Primary Set - Option 2 - Russian/Bulgarian
			0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x044b, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
			0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
			0x042e, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413, 0x0425, 0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e,
			0x041f, 0x042f, 0x0420, 0x0421, 0x0422, 0x0423, 0x0416, 0x0412, 0x042c, 0x042a, 0x0417, 0x0428, 0x042d, 0x0429, 0x0427, 0x042b,
			0x044e, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433, 0x0445, 0x0438, 0x0439, 0x043a, 0x043b, 0x043c, 0x043d, 0x043e,
			0x043f, 0x044f, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432, 0x044c, 0x044a, 0x0437, 0x0448, 0x044d, 0x0449, 0x0447, 0x044b
		},
		{
			// Cyrillic G0 Primary Set - Option 3 - Ukrainian
			0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x00ef, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
			0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
			0x042e, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413, 0x0425, 0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e,
			0x041f, 0x042f, 0x0420, 0x0421, 0x0422, 0x0423, 0x0416, 0x0412, 0x042c, 0x0049, 0x0417, 0x0428, 0x042d, 0x0429, 0x0427, 0x00cf,
			0x044e, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433, 0x0445, 0x0438, 0x0439, 0x043a, 0x043b, 0x043c, 0x043d, 0x043e,
			0x043f, 0x044f, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432, 0x044c, 0x0069, 0x0437, 0x0448, 0x044d, 0x0449, 0x0447, 0x00ff
		},
		{
			// Greek G0 Primary Set
			0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
			0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
			0x0390, 0x0391, 0x0392, 0x0393, 0x0394, 0x0395, 0x0396, 0x0397, 0x0398, 0x0399, 0x039a, 0x039b, 0x039c, 0x039d, 0x039e, 0x039f,
			0x03a0, 0x03a1, 0x03a2, 0x03a3, 0x03a4, 0x03a5, 0x03a6, 0x03a7, 0x03a8, 0x03a9, 0x03aa, 0x03ab, 0x03ac, 0x03ad, 0x03ae, 0x03af,
			0x03b0, 0x03b1, 0x03b2, 0x03b3, 0x03b4, 0x03b5, 0x03b6, 0x03b7, 0x03b8, 0x03b9, 0x03ba, 0x03bb, 0x03bc, 0x03bd, 0x03be, 0x03bf,
			0x03c0, 0x03c1, 0x03c2, 0x03c3, 0x03c4, 0x03c5, 0x03c6, 0x03c7, 0x03c8, 0x03c9, 0x03ca, 0x03cb, 0x03cc, 0x03cd, 0x03ce, 0x03cf
		}
		//{ // Arabic G0 Primary Set
		//},
		//{ // Hebrew G0 Primary Set
		//}
	};
};

uint16_t telx_to_ucs2(uint8_t c, TeletextState const& config) {
	if (Parity8[c] == 0) {
		config.host->log(Warning, format("Teletext: unrecoverable data error (5): %s", c).c_str());
		return 0x20;
	}

	uint16_t val = c & 0x7f;
	if (val >= 0x20) {
		val = config.G0[LATIN][val - 0x20];
	}
	return val;
}

void remap_g0_charset(uint8_t c, TeletextState &config) {
	if (c != config.primaryCharset.current) {
		uint8_t m = G0_LatinNationalSubsetsMap[c];
		if (m == 0xff) {
			config.host->log(Warning, format("Teletext: G0 subset %s.%s is not implemented", (c >> 3), (c & 0x7)).c_str());
		} else {
			for (uint8_t j = 0; j < 13; j++) {
				config.G0[LATIN][G0_LatinNationalSubsetsPositions[j]] = G0_LatinNationalSubsets[m].characters[j];
			}
			config.primaryCharset.current = c;
		}
	}
}

// entities, used in color mode, to replace unsafe HTML tag chars
struct Entity {
	uint16_t character;
	const char *entity;
};

Entity const entities[] = {
	{ '<', "&lt;" },
	{ '>', "&gt;" },
	{ '&', "&amp;" }
};

static bool isEmpty(PageBuffer const& pageIn) {
	for (int col = 0; col < COLS; col++) {
		for (int row = 1; row < ROWS; row++) {
			if (pageIn.text[row][col] == 0x0b)
				return false;
		}
	}
	return true;
}

static
void process_row(TeletextState const& config, const uint16_t* srcRow, Page* pageOut) {
	int colStart = COLS;
	int colStop = COLS;

	for (int col = COLS-1; col >= 0; col--) {
		if (srcRow[col] == 0xb) {
			colStart = col;
			break;
		}
	}
	if (colStart >= COLS)
		return; //empty line

	for (int col = colStart + 1; col < COLS; col++) {
		if (srcRow[col] > 0x20) {
			if (colStop >= COLS) colStart = col;
			colStop = col;
		}
		if (srcRow[col] == 0xa)
			break;
	}
	if (colStop >= COLS)
		return; //empty line

	// section 12.2: Alpha White ("Set-After") - Start-of-row default condition.
	uint8_t fgColor = 0x7; //white(7)
	uint8_t fontTagOpened = No;
	for (int col = 0; col <= colStop; col++) {
		uint16_t val = srcRow[col];
		if (col < colStart) {
			if (val <= 0x7)
				fgColor = (uint8_t)val;
		}
		if (col == colStart) {
			if ((fgColor != 0x7) && (config.colors == Yes)) {
				//TODO: look for "//colors:": fprintf(fout, "<font color=\"%s\">", TELX_Colors[fgColor]);
				fontTagOpened = Yes;
			}
		}

		if (col >= colStart) {
			if (val <= 0x7) {
				if (config.colors == Yes) {
					if (fontTagOpened == Yes) {
						//colors: fprintf(fout, "</font> ");
						fontTagOpened = No;
					}
					if ((val > 0x0) && (val < 0x7)) {
						//colors: fprintf(fout, "<font color=\"%s\">", TELX_Colors[v]);
						fontTagOpened = Yes;
					}
				} else {
					val = 0x20;
				}
			}

			if (val >= 0x20) {
				if (config.colors == Yes) {
					for(auto entity : entities) {
						if (val == entity.character) { // translate chars into entities when in color mode
							//colors: fprintf(fout, "%s", entity.entity);
							val = 0; // v < 0x20 won't be printed in next block
							break;
						}
					}
				}
			}

			if (val >= 0x20) {
				char u[4] {};
				ucs2_to_utf8(u, val);
				pageOut->lines.back() += u;
			}
		}
	}

	if ((config.colors == Yes) && (fontTagOpened == Yes)) {
		//colors: fprintf(fout, "</font>");
		fontTagOpened = No;
	}

	if (config.seMode == Yes) {
		pageOut->lines.back() += " ";
	} else {
		pageOut->lines.push_back({});
	}
}

std::unique_ptr<Page> process_page(TeletextState &config) {
	PageBuffer* pageIn = &config.pageBuffer;
	auto pageOut = make_unique<Page>();

	if (isEmpty(*pageIn))
		return pageOut;

	pageIn->hideTimestamp = std::max(pageIn->hideTimestamp, pageIn->showTimestamp);

	if (config.seMode == Yes) {
		++config.framesProduced;
		pageOut->tsInMs = pageIn->showTimestamp;
	} else {
		pageOut->showTimestamp = pageIn->showTimestamp;
		pageOut->hideTimestamp = pageIn->hideTimestamp;
	}

	for (int row = 1; row < 25; row++)
		process_row(config, pageIn->text[row], pageOut.get());

	return pageOut;
}

std::unique_ptr<Page> process_telx_packet(TeletextState &config, DataUnit dataUnitId, Payload *packet, uint64_t timestamp) {
	// section 7.1.2
	uint8_t address = (unham_8_4(packet->address[1]) << 4) | unham_8_4(packet->address[0]);
	uint8_t m = address & 0x7;
	if (m == 0)
		m = 8;
	uint8_t y = (address >> 3) & 0x1f;
	uint8_t designationCode = (y > 25) ? unham_8_4(packet->data[0]) : 0x00;
	std::unique_ptr<Page> pageOut;

	if (y == 0) {
		uint8_t i = (unham_8_4(packet->data[1]) << 4) | unham_8_4(packet->data[0]);
		uint8_t subtitleFlag = (unham_8_4(packet->data[5]) & 0x08) >> 3;
		config.cc_map[i] |= subtitleFlag << (m - 1);

		if ((config.page == 0) && (subtitleFlag == Yes) && (i < 0xff)) {
			config.page = (m << 8) | (unham_8_4(packet->data[1]) << 4) | unham_8_4(packet->data[0]);
		}

		uint16_t pageNum = (m << 8) | (unham_8_4(packet->data[1]) << 4) | unham_8_4(packet->data[0]);
		uint8_t charset = ((unham_8_4(packet->data[7]) & 0x08) | (unham_8_4(packet->data[7]) & 0x04) | (unham_8_4(packet->data[7]) & 0x02)) >> 1;

		// Section 9.3.1.3
		config.transmissionMode = (TransmissionMode)(unham_8_4(packet->data[7]) & 0x01);
		if ((config.transmissionMode == Parallel) && (dataUnitId != Subtitle))
			return nullptr;

		if ((config.receivingData == Yes) && (
		        ((config.transmissionMode == Serial) && (PAGE(pageNum) != PAGE(config.page))) ||
		        ((config.transmissionMode == Parallel) && (PAGE(pageNum) != PAGE(config.page)) && (m == MAGAZINE(config.page)))
		    )) {
			config.receivingData = No;
			return nullptr;
		}

		if (pageNum != config.page)
			return nullptr; //page transmission is terminated, however now we are waiting for our new page

		if (config.pageBuffer.tainted == Yes) { //begining of page transmission
			config.pageBuffer.hideTimestamp = timestamp - 40;
			pageOut = process_page(config);
		}

		config.pageBuffer.showTimestamp = timestamp;
		config.pageBuffer.hideTimestamp = 0;
		memset(config.pageBuffer.text, 0x00, sizeof(config.pageBuffer.text));
		config.pageBuffer.tainted = No;
		config.receivingData = Yes;
		config.primaryCharset.G0_X28 = Undef;

		uint8_t c = (config.primaryCharset.G0_M29 != Undef) ? config.primaryCharset.G0_M29 : charset;
		remap_g0_charset(c, config);
	} else if ((m == MAGAZINE(config.page)) && (y >= 1) && (y <= 23) && (config.receivingData == Yes)) {
		// Section 9.4.1
		for (uint8_t i = 0; i < 40; i++) {
			if (config.pageBuffer.text[y][i] == 0x00)
				config.pageBuffer.text[y][i] = telx_to_ucs2(packet->data[i], config);
		}
		config.pageBuffer.tainted = Yes;
	} else if ((m == MAGAZINE(config.page)) && (y == 26) && (config.receivingData == Yes)) {
		// Section 12.3.2
		uint8_t X26Row = 0, X26Col = 0;
		uint32_t triplets[13] = { 0 };
		for (uint8_t i = 1, j = 0; i < 40; i += 3, j++) {
			triplets[j] = unham_24_18((packet->data[i + 2] << 16) | (packet->data[i + 1] << 8) | packet->data[i]);
		}

		for (uint8_t j = 0; j < 13; j++) {
			if (triplets[j] == 0xffffffff) {
				config.host->log(Warning, format("Teletext: unrecoverable data error (1): %s", triplets[j]).c_str());
				continue;
			}

			uint8_t data = (triplets[j] & 0x3f800) >> 11;
			uint8_t mode = (triplets[j] & 0x7c0) >> 6;
			uint8_t address = triplets[j] & 0x3f;
			uint8_t row_address_group = (address >= 40) && (address <= 63);
			if ((mode == 0x04) && (row_address_group == Yes)) {
				X26Row = address - 40;
				if (X26Row == 0) X26Row = 24;
				X26Col = 0;
			}
			if ((mode >= 0x11) && (mode <= 0x1f) && (row_address_group == Yes))
				break; //termination marker

			if ((mode == 0x0f) && (row_address_group == No)) {
				X26Col = address;
				if (data > 31) config.pageBuffer.text[X26Row][X26Col] = G2[0][data - 0x20];
			}

			if ((mode >= 0x11) && (mode <= 0x1f) && (row_address_group == No)) {
				X26Col = address;
				if ((data >= 65) && (data <= 90)) { // A - Z
					config.pageBuffer.text[X26Row][X26Col] = G2_Accents[mode - 0x11][data - 65];
				} else if ((data >= 97) && (data <= 122)) { // a - z
					config.pageBuffer.text[X26Row][X26Col] = G2_Accents[mode - 0x11][data - 71];
				} else {
					config.pageBuffer.text[X26Row][X26Col] = telx_to_ucs2(data, config);
				}
			}
		}
	} else if ((m == MAGAZINE(config.page)) && (y == 28) && (config.receivingData == Yes)) {
		// Section 9.4.7
		if ((designationCode == 0) || (designationCode == 4)) {
			uint32_t triplet0 = unham_24_18((packet->data[3] << 16) | (packet->data[2] << 8) | packet->data[1]);
			if (triplet0 == 0xffffffff) {
				config.host->log(Warning, format("Teletext: unrecoverable data error (2): %s", triplet0).c_str());
			} else {
				if ((triplet0 & 0x0f) == 0x00) {
					config.primaryCharset.G0_X28 = (triplet0 & 0x3f80) >> 7;
					remap_g0_charset(config.primaryCharset.G0_X28, config);
				}
			}
		}
	} else if ((m == MAGAZINE(config.page)) && (y == 29)) {
		// Section 9.5.1
		if ((designationCode == 0) || (designationCode == 4)) {
			uint32_t triplet0 = unham_24_18((packet->data[3] << 16) | (packet->data[2] << 8) | packet->data[1]);
			if (triplet0 == 0xffffffff) {
				config.host->log(Warning, format("Teletext: unrecoverable data error (3): %s", triplet0).c_str());
			} else {
				if ((triplet0 & 0xff) == 0x00) {
					config.primaryCharset.G0_M29 = (triplet0 & 0x3f80) >> 7;
					if (config.primaryCharset.G0_X28 == Undef) {
						remap_g0_charset(config.primaryCharset.G0_M29, config);
					}
				}
			}
		}
	} else if ((m == 8) && (y == 30)) {
		// Section 9.8
		if (config.states.progInfoProcessed == No) {
			if (unham_8_4(packet->data[0]) < 2) {
				for (uint8_t i = 20; i < 40; i++) {
					uint8_t c = (uint8_t)telx_to_ucs2(packet->data[i], config);
					if (c < 0x20)
						continue;

					char u[4] {};
					ucs2_to_utf8(u, c);
				}

				//time conversion here is insane
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

				config.states.progInfoProcessed = Yes;
			}
		}
	}

	return pageOut;
}

}
