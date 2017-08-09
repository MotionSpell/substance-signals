#include "telx2ttml.hpp"
#include "lib_utils/time.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
}

//#define DEBUG_DISPLAY_TIMESTAMPS
#define USP_HACK

#ifdef USP_HACK
#define USP_HACK_OFFSET_IN_MS 0
#define USP_ROUNDUP(t) (((t % 1000) ? ((t / 1000 + 1) * 1000) : t) + USP_HACK_OFFSET_IN_MS)
#else
#define USP_ROUNDUP(t) (t)
#endif

namespace Modules {
namespace Transform {

const std::string Page::toTTML(uint64_t startTimeInMs, uint64_t endTimeInMs, uint64_t idx) const {
	std::stringstream ttml;
#ifndef DEBUG_DISPLAY_TIMESTAMPS
	if (!ss.str().empty())
#endif
	{
		const size_t timecodeSize = 24;
		char timecode_show[timecodeSize] = { 0 };
		timeInMsToStr(startTimeInMs, timecode_show, ".");
		timecode_show[timecodeSize-1] = 0;
		char timecode_hide[timecodeSize] = { 0 };
		timeInMsToStr(endTimeInMs, timecode_hide, ".");
		timecode_hide[timecodeSize-1] = 0;

		ttml << "      <p region=\"Region\" style=\"textAlignment_0\" begin=\"" << timecode_show << "\" end=\"" << timecode_hide << "\" xml:id=\"s" << idx << "\">\n";
#ifdef DEBUG_DISPLAY_TIMESTAMPS
		ttml << "        <span style=\"Style0_0\">" << timecode_show << " - " << timecode_hide << "</span>\n";
#else
		ttml << "        <span style=\"Style0_0\">" << ss.str() << "</span>\n";
#endif
		ttml << "      </p>\n";
	}
	return ttml.str();
}

const std::string Page::toSRT() {
		std::stringstream srt;
		{
			char buf[255];
			snprintf(buf, 255, "%.3f|", (double)tsInMs / 1000.0);
			srt << buf;
		}

		{
			char timecode_show[24] = { 0 };
			timeInMsToStr(show_timestamp, timecode_show);
			timecode_show[12] = 0;

			char timecode_hide[24] = { 0 };
			timeInMsToStr(hide_timestamp, timecode_hide);
			timecode_hide[12] = 0;

			char buf[255];
			snprintf(buf, 255, "%u\r\n%s --> %s\r\n", (unsigned)++frames_produced, timecode_show, timecode_hide);
			srt << buf;
		}

		srt << ss.str();

		return srt.str();
	}

const std::string TeletextToTTML::toTTML(uint64_t startTimeInMs, uint64_t endTimeInMs) {
	std::stringstream ttml;
	ttml << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n";
	ttml << "<tt xmlns=\"http://www.w3.org/ns/ttml\" xmlns:tt=\"http://www.w3.org/ns/ttml\" xmlns:ttm=\"http://www.w3.org/ns/ttml#metadata\" xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" xmlns:ttp=\"http://www.w3.org/ns/ttml#parameter\" xmlns:ebutts=\"urn:ebu:tt:style\" xmlns:ebuttm=\"urn:ebu:tt:metadata\" xml:lang=\"" << lang << "\" ttp:timeBase=\"media\">\n";
	ttml << "  <head>\n";
	ttml << "    <metadata>\n";
	ttml << "      <ebuttm:documentMetadata>\n";
	ttml << "        <ebuttm:conformsToStandard>urn:ebu:tt:distribution:2014-01</ebuttm:conformsToStandard>\n";
	ttml << "      </ebuttm:documentMetadata>\n";
	ttml << "    </metadata>\n";
	ttml << "    <styling>\n";
	ttml << "      <style xml:id=\"Style0_0\" tts:fontFamily=\"proportionalSansSerif\" tts:backgroundColor=\"#00000099\" tts:color=\"#FFFFFF\" tts:fontSize=\"100%\" tts:lineHeight=\"normal\" ebutts:linePadding=\"0.5c\" />\n";
	ttml << "      <style xml:id=\"textAlignment_0\" tts:textAlign=\"center\" />\n";
	ttml << "    </styling>\n";
	ttml << "    <layout>\n";
	ttml << "      <region xml:id=\"Region\" tts:origin=\"10% 10%\" tts:extent=\"80% 80%\" tts:displayAlign=\"after\" />\n";
	ttml << "    </layout>\n";
	ttml << "  </head>\n";
	ttml << "  <body>\n";
	ttml << "    <div>\n";

	int64_t offset;
	switch (timingPolicy) {
	case AbsoluteUTC: offset = USP_ROUNDUP((int64_t)firstDataAbsTimeInMs); break;
	case RelativeToMedia: offset = 0; break;
	case RelativeToSplit: offset = -1 * startTimeInMs; break;
	default: throw error("Unknown timing policy (1)");
	}

#ifdef DEBUG_DISPLAY_TIMESTAMPS
	auto pageOut = uptr(new Page);
	ttml << pageOut->toTTML(offset + startTimeInMs, offset + endTimeInMs, startTimeInMs / clockToTimescale(this->splitDurationIn180k, 1000));
#else
	auto page = currentPages.begin();
	while (page != currentPages.end()) {
		if ((*page)->endTimeInMs > startTimeInMs && (*page)->startTimeInMs < endTimeInMs) {
			auto localStartTimeInMs = std::max<uint64_t>((*page)->startTimeInMs, startTimeInMs);
			auto localEndTimeInMs = std::min<uint64_t>((*page)->endTimeInMs, endTimeInMs);
			log(Debug, "[%s-%s]: %s - %s: %s", startTimeInMs, endTimeInMs, localStartTimeInMs, localEndTimeInMs, (*page)->ss.str());
			ttml << (*page)->toTTML(localStartTimeInMs + offset, localEndTimeInMs + offset, startTimeInMs / clockToTimescale(this->splitDurationIn180k, 1000));
		}
		if ((*page)->endTimeInMs <= endTimeInMs) {
			page = currentPages.erase(page);
		} else {
			++page;
		}
	}
#endif /*DEBUG_DISPLAY_TIMESTAMPS*/

	ttml << "    </div>\n";
	ttml << "  </body>\n";
	ttml << "</tt>\n\n";
	return ttml.str();
}

namespace {

typedef enum {
	NO = 0x00,
	YES = 0x01,
	UNDEF = 0xff
} bool_t;

typedef struct {
	uint8_t current;
	uint8_t g0_m29;
	uint8_t g0_x28;
} PrimaryCharset;
PrimaryCharset primary_charset = { 0x00, UNDEF, UNDEF };

// application states -- flags for notices that should be printed only once
typedef struct {
	uint8_t programme_info_processed;
	uint8_t pts_initialized;
} State;
State states = { NO, NO };

// helper, array length function
#define ARRAY_LENGTH(a) (sizeof(a)/sizeof(a[0]))

// entities, used in colour mode, to replace unsafe HTML tag chars
typedef struct {
	uint16_t character;
	char *entity;
} Entity;
Entity const ENTITIES[] = {
	{ '<', "&lt;" },
	{ '>', "&gt;" },
	{ '&', "&amp;" }
};

//const char* TTXT_COLOURS[8] = {
//	//black,     red,       green,     yellow,    blue,      magenta,   cyan,      white
//	"#000000", "#ff0000", "#00ff00", "#ffff00", "#0000ff", "#ff00ff", "#00ffff", "#ffffff"
//};

// SRT frames produced
uint32_t frames_produced = 0;

#define VERBOSE_ONLY if(0) 
#define STDERR_ONLY if(0) 

const uint8_t PARITY_8[256] = {
	0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00,
	0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01,
	0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01,
	0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00,
	0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01,
	0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00,
	0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00,
	0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01,
	0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01,
	0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00,
	0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00,
	0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01,
	0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00,
	0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01,
	0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01,
	0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00
};

const uint8_t REVERSE_8[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

const uint8_t UNHAM_8_4[256] = {
	0x01, 0xff, 0x01, 0x01, 0xff, 0x00, 0x01, 0xff, 0xff, 0x02, 0x01, 0xff, 0x0a, 0xff, 0xff, 0x07,
	0xff, 0x00, 0x01, 0xff, 0x00, 0x00, 0xff, 0x00, 0x06, 0xff, 0xff, 0x0b, 0xff, 0x00, 0x03, 0xff,
	0xff, 0x0c, 0x01, 0xff, 0x04, 0xff, 0xff, 0x07, 0x06, 0xff, 0xff, 0x07, 0xff, 0x07, 0x07, 0x07,
	0x06, 0xff, 0xff, 0x05, 0xff, 0x00, 0x0d, 0xff, 0x06, 0x06, 0x06, 0xff, 0x06, 0xff, 0xff, 0x07,
	0xff, 0x02, 0x01, 0xff, 0x04, 0xff, 0xff, 0x09, 0x02, 0x02, 0xff, 0x02, 0xff, 0x02, 0x03, 0xff,
	0x08, 0xff, 0xff, 0x05, 0xff, 0x00, 0x03, 0xff, 0xff, 0x02, 0x03, 0xff, 0x03, 0xff, 0x03, 0x03,
	0x04, 0xff, 0xff, 0x05, 0x04, 0x04, 0x04, 0xff, 0xff, 0x02, 0x0f, 0xff, 0x04, 0xff, 0xff, 0x07,
	0xff, 0x05, 0x05, 0x05, 0x04, 0xff, 0xff, 0x05, 0x06, 0xff, 0xff, 0x05, 0xff, 0x0e, 0x03, 0xff,
	0xff, 0x0c, 0x01, 0xff, 0x0a, 0xff, 0xff, 0x09, 0x0a, 0xff, 0xff, 0x0b, 0x0a, 0x0a, 0x0a, 0xff,
	0x08, 0xff, 0xff, 0x0b, 0xff, 0x00, 0x0d, 0xff, 0xff, 0x0b, 0x0b, 0x0b, 0x0a, 0xff, 0xff, 0x0b,
	0x0c, 0x0c, 0xff, 0x0c, 0xff, 0x0c, 0x0d, 0xff, 0xff, 0x0c, 0x0f, 0xff, 0x0a, 0xff, 0xff, 0x07,
	0xff, 0x0c, 0x0d, 0xff, 0x0d, 0xff, 0x0d, 0x0d, 0x06, 0xff, 0xff, 0x0b, 0xff, 0x0e, 0x0d, 0xff,
	0x08, 0xff, 0xff, 0x09, 0xff, 0x09, 0x09, 0x09, 0xff, 0x02, 0x0f, 0xff, 0x0a, 0xff, 0xff, 0x09,
	0x08, 0x08, 0x08, 0xff, 0x08, 0xff, 0xff, 0x09, 0x08, 0xff, 0xff, 0x0b, 0xff, 0x0e, 0x03, 0xff,
	0xff, 0x0c, 0x0f, 0xff, 0x04, 0xff, 0xff, 0x09, 0x0f, 0xff, 0x0f, 0x0f, 0xff, 0x0e, 0x0f, 0xff,
	0x08, 0xff, 0xff, 0x05, 0xff, 0x0e, 0x0d, 0xff, 0xff, 0x0e, 0x0f, 0xff, 0x0e, 0x0e, 0xff, 0x0e
};

typedef enum {
	LATIN = 0,
	CYRILLIC1,
	CYRILLIC2,
	CYRILLIC3,
	GREEK,
	ARABIC,
	HEBREW
} g0_charsets_t;

// Note: All characters are encoded in UCS-2

// --- G0 ----------------------------------------------------------------------

// G0 charsets
uint16_t G0[5][96] = {
	{ // Latin G0 Primary Set
		0x0020, 0x0021, 0x0022, 0x00a3, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
		0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
		0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
		0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005a, 0x00ab, 0x00bd, 0x00bb, 0x005e, 0x0023,
		0x002d, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
		0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x00bc, 0x00a6, 0x00be, 0x00f7, 0x007f
	},
	{ // Cyrillic G0 Primary Set - Option 1 - Serbian/Croatian
		0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x044b, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
		0x0030, 0x0031, 0x3200, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
		0x0427, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413, 0x0425, 0x0418, 0x0408, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e,
		0x041f, 0x040c, 0x0420, 0x0421, 0x0422, 0x0423, 0x0412, 0x0403, 0x0409, 0x040a, 0x0417, 0x040b, 0x0416, 0x0402, 0x0428, 0x040f,
		0x0447, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433, 0x0445, 0x0438, 0x0428, 0x043a, 0x043b, 0x043c, 0x043d, 0x043e,
		0x043f, 0x042c, 0x0440, 0x0441, 0x0442, 0x0443, 0x0432, 0x0423, 0x0429, 0x042a, 0x0437, 0x042b, 0x0436, 0x0422, 0x0448, 0x042f
	},
	{ // Cyrillic G0 Primary Set - Option 2 - Russian/Bulgarian
		0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x044b, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
		0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
		0x042e, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413, 0x0425, 0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e,
		0x041f, 0x042f, 0x0420, 0x0421, 0x0422, 0x0423, 0x0416, 0x0412, 0x042c, 0x042a, 0x0417, 0x0428, 0x042d, 0x0429, 0x0427, 0x042b,
		0x044e, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433, 0x0445, 0x0438, 0x0439, 0x043a, 0x043b, 0x043c, 0x043d, 0x043e,
		0x043f, 0x044f, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432, 0x044c, 0x044a, 0x0437, 0x0448, 0x044d, 0x0449, 0x0447, 0x044b
	},
	{ // Cyrillic G0 Primary Set - Option 3 - Ukrainian
		0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x00ef, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
		0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
		0x042e, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413, 0x0425, 0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e,
		0x041f, 0x042f, 0x0420, 0x0421, 0x0422, 0x0423, 0x0416, 0x0412, 0x042c, 0x0049, 0x0417, 0x0428, 0x042d, 0x0429, 0x0427, 0x00cf,
		0x044e, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433, 0x0445, 0x0438, 0x0439, 0x043a, 0x043b, 0x043c, 0x043d, 0x043e,
		0x043f, 0x044f, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432, 0x044c, 0x0069, 0x0437, 0x0448, 0x044d, 0x0449, 0x0447, 0x00ff
	},
	{ // Greek G0 Primary Set
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

// array positions where chars from G0_LATIN_NATIONAL_SUBSETS are injected into G0[LATIN]
const uint8_t G0_LATIN_NATIONAL_SUBSETS_POSITIONS[13] = {
	0x03, 0x04, 0x20, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x5b, 0x5c, 0x5d, 0x5e
};

// ETS 300 706, chapter 15.2, table 32: Function of Default G0 and G2 Character Set Designation
// and National Option Selection bits in packets X/28/0 Format 1, X/28/4, M/29/0 and M/29/4

// Latin National Option Sub-sets
struct {
	const char *language;
	uint16_t characters[13];
} const G0_LATIN_NATIONAL_SUBSETS[14] = {
	{ // 0
		"English",
		{ 0x00a3, 0x0024, 0x0040, 0x00ab, 0x00bd, 0x00bb, 0x005e, 0x0023, 0x002d, 0x00bc, 0x00a6, 0x00be, 0x00f7 }
	},
	{ // 1
		"French",
		{ 0x00e9, 0x00ef, 0x00e0, 0x00eb, 0x00ea, 0x00f9, 0x00ee, 0x0023, 0x00e8, 0x00e2, 0x00f4, 0x00fb, 0x00e7 }
	},
	{ // 2
		"Swedish, Finnish, Hungarian",
		{ 0x0023, 0x00a4, 0x00c9, 0x00c4, 0x00d6, 0x00c5, 0x00dc, 0x005f, 0x00e9, 0x00e4, 0x00f6, 0x00e5, 0x00fc }
	},
	{ // 3
		"Czech, Slovak",
		{ 0x0023, 0x016f, 0x010d, 0x0165, 0x017e, 0x00fd, 0x00ed, 0x0159, 0x00e9, 0x00e1, 0x011b, 0x00fa, 0x0161 }
	},
	{ // 4
		"German",
		{ 0x0023, 0x0024, 0x00a7, 0x00c4, 0x00d6, 0x00dc, 0x005e, 0x005f, 0x00b0, 0x00e4, 0x00f6, 0x00fc, 0x00df }
	},
	{ // 5
		"Portuguese, Spanish",
		{ 0x00e7, 0x0024, 0x00a1, 0x00e1, 0x00e9, 0x00ed, 0x00f3, 0x00fa, 0x00bf, 0x00fc, 0x00f1, 0x00e8, 0x00e0 }
	},
	{ // 6
		"Italian",
		{ 0x00a3, 0x0024, 0x00e9, 0x00b0, 0x00e7, 0x00bb, 0x005e, 0x0023, 0x00f9, 0x00e0, 0x00f2, 0x00e8, 0x00ec }
	},
	{ // 7
		"Rumanian",
		{ 0x0023, 0x00a4, 0x0162, 0x00c2, 0x015e, 0x0102, 0x00ce, 0x0131, 0x0163, 0x00e2, 0x015f, 0x0103, 0x00ee }
	},
	{ // 8
		"Polish",
		{ 0x0023, 0x0144, 0x0105, 0x017b, 0x015a, 0x0141, 0x0107, 0x00f3, 0x0119, 0x017c, 0x015b, 0x0142, 0x017a }
	},
	{ // 9
		"Turkish",
		{ 0x0054, 0x011f, 0x0130, 0x015e, 0x00d6, 0x00c7, 0x00dc, 0x011e, 0x0131, 0x015f, 0x00f6, 0x00e7, 0x00fc }
	},
	{ // a
		"Serbian, Croatian, Slovenian",
		{ 0x0023, 0x00cb, 0x010c, 0x0106, 0x017d, 0x0110, 0x0160, 0x00eb, 0x010d, 0x0107, 0x017e, 0x0111, 0x0161 }
	},
	{ // b
		"Estonian",
		{ 0x0023, 0x00f5, 0x0160, 0x00c4, 0x00d6, 0x017e, 0x00dc, 0x00d5, 0x0161, 0x00e4, 0x00f6, 0x017e, 0x00fc }
	},
	{ // c
		"Lettish, Lithuanian",
		{ 0x0023, 0x0024, 0x0160, 0x0117, 0x0119, 0x017d, 0x010d, 0x016b, 0x0161, 0x0105, 0x0173, 0x017e, 0x012f }
	}
};

// References to the G0_LATIN_NATIONAL_SUBSETS array
const uint8_t G0_LATIN_NATIONAL_SUBSETS_MAP[56] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x01, 0x02, 0x03, 0x04, 0xff, 0x06, 0xff,
	0x00, 0x01, 0x02, 0x09, 0x04, 0x05, 0x06, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0xff, 0x07,
	0xff, 0xff, 0x0b, 0x03, 0x04, 0xff, 0x0c, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0x09, 0xff, 0xff, 0xff, 0xff
};

// --- G2 ----------------------------------------------------------------------

const uint16_t G2[1][96] = {
	{ // Latin G2 Supplementary Set
		0x0020, 0x00a1, 0x00a2, 0x00a3, 0x0024, 0x00a5, 0x0023, 0x00a7, 0x00a4, 0x2018, 0x201c, 0x00ab, 0x2190, 0x2191, 0x2192, 0x2193,
		0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00d7, 0x00b5, 0x00b6, 0x00b7, 0x00f7, 0x2019, 0x201d, 0x00bb, 0x00bc, 0x00bd, 0x00be, 0x00bf,
		0x0020, 0x0300, 0x0301, 0x0302, 0x0303, 0x0304, 0x0306, 0x0307, 0x0308, 0x0000, 0x030a, 0x0327, 0x005f, 0x030b, 0x0328, 0x030c,
		0x2015, 0x00b9, 0x00ae, 0x00a9, 0x2122, 0x266a, 0x20ac, 0x2030, 0x03B1, 0x0000, 0x0000, 0x0000, 0x215b, 0x215c, 0x215d, 0x215e,
		0x03a9, 0x00c6, 0x0110, 0x00aa, 0x0126, 0x0000, 0x0132, 0x013f, 0x0141, 0x00d8, 0x0152, 0x00ba, 0x00de, 0x0166, 0x014a, 0x0149,
		0x0138, 0x00e6, 0x0111, 0x00f0, 0x0127, 0x0131, 0x0133, 0x0140, 0x0142, 0x00f8, 0x0153, 0x00df, 0x00fe, 0x0167, 0x014b, 0x0020
	}
	//	{ // Cyrillic G2 Supplementary Set
	//	},
	//	{ // Greek G2 Supplementary Set
	//	},
	//	{ // Arabic G2 Supplementary Set
	//	}
};

const uint16_t G2_ACCENTS[15][52] = {
	// A B C D E F G H I J K L M N O P Q R S T U V W X Y Z a b c d e f g h i j k l m n o p q r s t u v w x y z
	{ // grave
		0x00c0, 0x0000, 0x0000, 0x0000, 0x00c8, 0x0000, 0x0000, 0x0000, 0x00cc, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00d2, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x00d9, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00e0, 0x0000, 0x0000, 0x0000, 0x00e8, 0x0000,
		0x0000, 0x0000, 0x00ec, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00f2, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00f9, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000
	},
	{ // acute
		0x00c1, 0x0000, 0x0106, 0x0000, 0x00c9, 0x0000, 0x0000, 0x0000, 0x00cd, 0x0000, 0x0000, 0x0139, 0x0000, 0x0143, 0x00d3, 0x0000,
		0x0000, 0x0154, 0x015a, 0x0000, 0x00da, 0x0000, 0x0000, 0x0000, 0x00dd, 0x0179, 0x00e1, 0x0000, 0x0107, 0x0000, 0x00e9, 0x0000,
		0x0123, 0x0000, 0x00ed, 0x0000, 0x0000, 0x013a, 0x0000, 0x0144, 0x00f3, 0x0000, 0x0000, 0x0155, 0x015b, 0x0000, 0x00fa, 0x0000,
		0x0000, 0x0000, 0x00fd, 0x017a
	},
	{ // circumflex
		0x00c2, 0x0000, 0x0108, 0x0000, 0x00ca, 0x0000, 0x011c, 0x0124, 0x00ce, 0x0134, 0x0000, 0x0000, 0x0000, 0x0000, 0x00d4, 0x0000,
		0x0000, 0x0000, 0x015c, 0x0000, 0x00db, 0x0000, 0x0174, 0x0000, 0x0176, 0x0000, 0x00e2, 0x0000, 0x0109, 0x0000, 0x00ea, 0x0000,
		0x011d, 0x0125, 0x00ee, 0x0135, 0x0000, 0x0000, 0x0000, 0x0000, 0x00f4, 0x0000, 0x0000, 0x0000, 0x015d, 0x0000, 0x00fb, 0x0000,
		0x0175, 0x0000, 0x0177, 0x0000
	},
	{ // tilde
		0x00c3, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0128, 0x0000, 0x0000, 0x0000, 0x0000, 0x00d1, 0x00d5, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0168, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00e3, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0129, 0x0000, 0x0000, 0x0000, 0x0000, 0x00f1, 0x00f5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0169, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000
	},
	{ // macron
		0x0100, 0x0000, 0x0000, 0x0000, 0x0112, 0x0000, 0x0000, 0x0000, 0x012a, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x014c, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x016a, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0101, 0x0000, 0x0000, 0x0000, 0x0113, 0x0000,
		0x0000, 0x0000, 0x012b, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x014d, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x016b, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000
	},
	{ // breve
		0x0102, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x011e, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x016c, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0103, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x011f, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x016d, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000
	},
	{ // dot
		0x0000, 0x0000, 0x010a, 0x0000, 0x0116, 0x0000, 0x0120, 0x0000, 0x0130, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x017b, 0x0000, 0x0000, 0x010b, 0x0000, 0x0117, 0x0000,
		0x0121, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x017c
	},
	{ // umlaut
		0x00c4, 0x0000, 0x0000, 0x0000, 0x00cb, 0x0000, 0x0000, 0x0000, 0x00cf, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00d6, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x00dc, 0x0000, 0x0000, 0x0000, 0x0178, 0x0000, 0x00e4, 0x0000, 0x0000, 0x0000, 0x00eb, 0x0000,
		0x0000, 0x0000, 0x00ef, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00f6, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00fc, 0x0000,
		0x0000, 0x0000, 0x00ff, 0x0000
	},
	{ 0 },
	{ // ring
		0x00c5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x016e, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00e5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x016f, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000
	},
	{ // cedilla
		0x0000, 0x0000, 0x00c7, 0x0000, 0x0000, 0x0000, 0x0122, 0x0000, 0x0000, 0x0000, 0x0136, 0x013b, 0x0000, 0x0145, 0x0000, 0x0000,
		0x0000, 0x0156, 0x015e, 0x0162, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00e7, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0137, 0x013c, 0x0000, 0x0146, 0x0000, 0x0000, 0x0000, 0x0157, 0x015f, 0x0163, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000
	},
	{ 0 },
	{ // double acute
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0150, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0170, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0151, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0171, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000
	},
	{ // ogonek
		0x0104, 0x0000, 0x0000, 0x0000, 0x0118, 0x0000, 0x0000, 0x0000, 0x012e, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0172, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0105, 0x0000, 0x0000, 0x0000, 0x0119, 0x0000,
		0x0000, 0x0000, 0x012f, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0173, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000
	},
	{ // caron
		0x0000, 0x0000, 0x010c, 0x010e, 0x011a, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x013d, 0x0000, 0x0147, 0x0000, 0x0000,
		0x0000, 0x0158, 0x0160, 0x0164, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x017d, 0x0000, 0x0000, 0x010d, 0x010f, 0x011b, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x013e, 0x0000, 0x0148, 0x0000, 0x0000, 0x0000, 0x0159, 0x0161, 0x0165, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x017e
	}
};
typedef enum {
	DATA_UNIT_EBU_TELETEXT_NONSUBTITLE = 0x02,
	DATA_UNIT_EBU_TELETEXT_SUBTITLE = 0x03,
	DATA_UNIT_EBU_TELETEXT_INVERTED = 0x0c,
	DATA_UNIT_VPS = 0xc3,
	DATA_UNIT_CLOSED_CAPTIONS = 0xc5
} data_unit_t;

typedef struct {
	uint8_t _clock_in; // clock run in
	uint8_t _framing_code; // framing code, not needed, ETSI 300 706: const 0xe4
	uint8_t address[2];
	uint8_t data[40];
} teletext_packet_payload_t;

uint8_t unham_8_4(uint8_t a) {
	uint8_t r = UNHAM_8_4[a];
	if (r == 0xff) {
		r = 0;
		VERBOSE_ONLY fprintf(stderr, "! Unrecoverable data error; UNHAM8/4(%02x)\n", a);
	}
	return (r & 0x0f);
}

// extracts magazine number from teletext page
#define MAGAZINE(p) ((p >> 8) & 0xf)

// extracts page number from teletext page
#define PAGE(p) (p & 0xff)

// ETS 300 706, chapter 8.3
uint32_t unham_24_18(uint32_t a) {
	uint8_t test = 0;

	// Tests A-F correspond to bits 0-6 respectively in 'test'.
	for (uint8_t i = 0; i < 23; i++) test ^= ((a >> i) & 0x01) * (i + 33);
	// Only parity bit is tested for bit 24
	test ^= ((a >> 23) & 0x01) * 32;

	if ((test & 0x1f) != 0x1f) {
		// Not all tests A-E correct
		if ((test & 0x20) == 0x20) {
			// F correct: Double error
			return 0xffffffff;
		}
		// Test F incorrect: Single error
		a ^= 1 << (30 - test);
	}

	return (a & 0x000004) >> 2 | (a & 0x000070) >> 3 | (a & 0x007f00) >> 4 | (a & 0x7f0000) >> 5;
}

// subtitle type pages bitmap, 2048 bits = 2048 possible pages in teletext (excl. subpages)
uint8_t cc_map[256] = { 0 };

// UCS-2 (16 bits) to UTF-8 (Unicode Normalization Form C (NFC)) conversion
void ucs2_to_utf8(char *r, uint16_t ch) {
	if (ch < 0x80) {
		r[0] = ch & 0x7f;
		r[1] = 0;
		r[2] = 0;
		r[3] = 0;
	}
	else if (ch < 0x800) {
		r[0] = (ch >> 6) | 0xc0;
		r[1] = (ch & 0x3f) | 0x80;
		r[2] = 0;
		r[3] = 0;
	}
	else {
		r[0] = (ch >> 12) | 0xe0;
		r[1] = ((ch >> 6) & 0x3f) | 0x80;
		r[2] = (ch & 0x3f) | 0x80;
		r[3] = 0;
	}
}

// check parity and translate any reasonable teletext character into ucs2
uint16_t telx_to_ucs2(uint8_t c) {
	if (PARITY_8[c] == 0) {
		VERBOSE_ONLY fprintf(stderr, "! Unrecoverable data error; PARITY(%02x)\n", c);
		return 0x20;
	}

	uint16_t r = c & 0x7f;
	if (r >= 0x20) r = G0[LATIN][r - 0x20];
	return r;
}

typedef enum {
	TRANSMISSION_MODE_PARALLEL = 0,
	TRANSMISSION_MODE_SERIAL = 1
} transmission_mode_t;

// teletext transmission mode
transmission_mode_t transmission_mode = TRANSMISSION_MODE_SERIAL;

// flag indicating if incoming data should be processed or ignored
uint8_t receiving_data = NO;

// application config global variable
typedef struct {
#ifdef __MINGW32__
	wchar_t *input_name; // input file name (used on Windows, UNICODE)
	wchar_t *output_name; // output file name (used on Windows, UNICODE)
#else
	char *input_name; // input file name
	char *output_name; // output file name
#endif
	uint8_t verbose; // should telxcc be verbose?
	uint16_t page; // teletext page containing cc we want to filter
	uint16_t tid; // 13-bit packet ID for teletext stream
	double offset; // time offset in seconds
	uint8_t colours; // output <font...></font> tags
	uint8_t bom; // print UTF-8 BOM characters at the beginning of output
	uint8_t nonempty; // produce at least one (dummy) frame
	uint64_t utc_refvalue; // UTC referential value
							// FIXME: move SE_MODE to output module
	uint8_t se_mode;
	//char *template; // output format template
	uint8_t m2ts; // consider input stream is af s M2TS, instead of TS
} Config;
Config config = {
	NULL,
	NULL,
	NO,
	0,
	0,
	0,
	NO,
	YES,
	NO,
	0,
	NO,
	//.template = NULL,
	NO
};

typedef struct {
	uint64_t show_timestamp; // show at timestamp (in ms)
	uint64_t hide_timestamp; // hide at timestamp (in ms)
	uint16_t text[25][40]; // 25 lines x 40 cols (1 screen/page) of wide chars
	uint8_t tainted; // 1 = text variable contains any data
} teletext_page_t;

// working teletext page buffer
teletext_page_t page_buffer;

void remap_g0_charset(uint8_t c) {
	if (c != primary_charset.current) {
		uint8_t m = G0_LATIN_NATIONAL_SUBSETS_MAP[c];
		if (m == 0xff) {
			STDERR_ONLY fprintf(stderr, "- G0 Latin National Subset ID 0x%1x.%1x is not implemented\n", (c >> 3), (c & 0x7));
		}
		else {
			for (uint8_t j = 0; j < 13; j++) G0[LATIN][G0_LATIN_NATIONAL_SUBSETS_POSITIONS[j]] = G0_LATIN_NATIONAL_SUBSETS[m].characters[j];
			VERBOSE_ONLY fprintf(stderr, "- Using G0 Latin National Subset ID 0x%1x.%1x (%s)\n", (c >> 3), (c & 0x7), G0_LATIN_NATIONAL_SUBSETS[m].language);
			primary_charset.current = c;
}
}
}

std::unique_ptr<Page> process_page(teletext_page_t *pageIn) {
#ifdef DEBUG
	for (uint8_t row = 1; row < 25; row++) {
		fprintf(fout, "# DEBUG[%02u]: ", row);
		for (uint8_t col = 0; col < 40; col++) fprintf(fout, "%3x ", pageIn->text[row][col]);
		fprintf(fout, "\n");
	}
	fprintf(fout, "\n");
#endif
	auto pageOut = uptr(new Page);

	// optimization: slicing column by column -- higher probability we could find boxed area start mark sooner
	uint8_t page_is_empty = YES;
	for (uint8_t col = 0; col < 40; col++) {
		for (uint8_t row = 1; row < 25; row++) {
			if (pageIn->text[row][col] == 0x0b) {
				page_is_empty = NO;
				goto page_is_empty;
			}
		}
	}
page_is_empty:
	if (page_is_empty == YES) return pageOut;

	if (pageIn->show_timestamp > pageIn->hide_timestamp) pageIn->hide_timestamp = pageIn->show_timestamp;

	if (config.se_mode == YES) {
		++frames_produced;
		pageOut->tsInMs = pageIn->show_timestamp;
	}
	else {
		pageOut->show_timestamp = pageIn->show_timestamp;
		pageOut->hide_timestamp = pageIn->hide_timestamp;
	}

	// process data
	for (uint8_t row = 1; row < 25; row++) {
		// anchors for string trimming purpose
		uint8_t col_start = 40;
		uint8_t col_stop = 40;

		for (int8_t col = 39; col >= 0; col--) {
			if (pageIn->text[row][col] == 0xb) {
				col_start = col;
				break;
			}
		}
		// line is empty
		if (col_start > 39) continue;

		for (uint8_t col = col_start + 1; col <= 39; col++) {
			if (pageIn->text[row][col] > 0x20) {
				if (col_stop > 39) col_start = col;
				col_stop = col;
			}
			if (pageIn->text[row][col] == 0xa) break;
		}
		// line is empty
		if (col_stop > 39) continue;

		// ETS 300 706, chapter 12.2: Alpha White ("Set-After") - Start-of-row default condition.
		// used for colour changes _before_ start box mark
		// white is default as stated in ETS 300 706, chapter 12.2
		// black(0), red(1), green(2), yellow(3), blue(4), magenta(5), cyan(6), white(7)
		uint8_t foreground_color = 0x7;
		uint8_t font_tag_opened = NO;

		for (uint8_t col = 0; col <= col_stop; col++) {
			// v is just a shortcut
			uint16_t v = pageIn->text[row][col];

			if (col < col_start) {
				if (v <= 0x7) foreground_color = (uint8_t)v;
			}

			if (col == col_start) {
				if ((foreground_color != 0x7) && (config.colours == YES)) {
					//TODO: look for "//colors:": fprintf(fout, "<font color=\"%s\">", TTXT_COLOURS[foreground_color]);
					font_tag_opened = YES;
				}
			}

			if (col >= col_start) {
				if (v <= 0x7) {
					// ETS 300 706, chapter 12.2: Unless operating in "Hold Mosaics" mode,
					// each character space occupied by a spacing attribute is displayed as a SPACE.
					if (config.colours == YES) {
						if (font_tag_opened == YES) {
							//colors: fprintf(fout, "</font> ");
							font_tag_opened = NO;
						}

						// black is considered as white for telxcc purpose
						// telxcc writes <font/> tags only when needed
						if ((v > 0x0) && (v < 0x7)) {
							//colors: fprintf(fout, "<font color=\"%s\">", TTXT_COLOURS[v]);
							font_tag_opened = YES;
						}
					}
					else v = 0x20;
				}

				if (v >= 0x20) {
					// translate some chars into entities, if in colour mode
					if (config.colours == YES) {
						for (uint8_t i = 0; i < ARRAY_LENGTH(ENTITIES); i++) {
							if (v == ENTITIES[i].character) {
								//colors: fprintf(fout, "%s", ENTITIES[i].entity);
								// v < 0x20 won't be printed in next block
								v = 0;
								break;
							}
						}
					}
				}

				if (v >= 0x20) {
					char u[4] = { 0, 0, 0, 0 };
					ucs2_to_utf8(u, v);
					pageOut->ss << u;
				}
			}
		}

		// no tag will left opened!
		if ((config.colours == YES) && (font_tag_opened == YES)) {
			//colors: fprintf(fout, "</font>");
			font_tag_opened = NO;
		}

		// line delimiter
		pageOut->ss << ((config.se_mode == YES) ? " " : "\r\n");
	}

	pageOut->ss << "\r\n";
	pageOut->ss.flush();
	return pageOut;
}

std::unique_ptr<Page> process_telx_packet(data_unit_t data_unit_id, teletext_packet_payload_t *packet, uint64_t timestamp) {
	// variable names conform to ETS 300 706, chapter 7.1.2
	uint8_t address = (unham_8_4(packet->address[1]) << 4) | unham_8_4(packet->address[0]);
	uint8_t m = address & 0x7;
	if (m == 0) m = 8;
	uint8_t y = (address >> 3) & 0x1f;
	uint8_t designation_code = (y > 25) ? unham_8_4(packet->data[0]) : 0x00;
	std::unique_ptr<Page> pageOut;

	if (y == 0) {
		// CC map
		uint8_t i = (unham_8_4(packet->data[1]) << 4) | unham_8_4(packet->data[0]);
		uint8_t flag_subtitle = (unham_8_4(packet->data[5]) & 0x08) >> 3;
		cc_map[i] |= flag_subtitle << (m - 1);

		if ((config.page == 0) && (flag_subtitle == YES) && (i < 0xff)) {
			config.page = (m << 8) | (unham_8_4(packet->data[1]) << 4) | unham_8_4(packet->data[0]);
			STDERR_ONLY fprintf(stderr, "- No teletext page specified, first received suitable page is %03x, not guaranteed\n", config.page);
		}

		// Page number and control bits
		uint16_t page_number = (m << 8) | (unham_8_4(packet->data[1]) << 4) | unham_8_4(packet->data[0]);
		uint8_t charset = ((unham_8_4(packet->data[7]) & 0x08) | (unham_8_4(packet->data[7]) & 0x04) | (unham_8_4(packet->data[7]) & 0x02)) >> 1;
		//uint8_t flag_suppress_header = unham_8_4(packet->data[6]) & 0x01;
		//uint8_t flag_inhibit_display = (unham_8_4(packet->data[6]) & 0x08) >> 3;

		// ETS 300 706, chapter 9.3.1.3:
		// When set to '1' the service is designated to be in Serial mode and the transmission of a page is terminated
		// by the next page header with a different page number.
		// When set to '0' the service is designated to be in Parallel mode and the transmission of a page is terminated
		// by the next page header with a different page number but the same magazine number.
		// The same setting shall be used for all page headers in the service.
		// ETS 300 706, chapter 7.2.1: Page is terminated by and excludes the next page header packet
		// having the same magazine address in parallel transmission mode, or any magazine address in serial transmission mode.
		transmission_mode = (transmission_mode_t)(unham_8_4(packet->data[7]) & 0x01);

		// FIXME: Well, this is not ETS 300 706 kosher, however we are interested in DATA_UNIT_EBU_TELETEXT_SUBTITLE only
		if ((transmission_mode == TRANSMISSION_MODE_PARALLEL) && (data_unit_id != DATA_UNIT_EBU_TELETEXT_SUBTITLE)) return nullptr;

		if ((receiving_data == YES) && (
			((transmission_mode == TRANSMISSION_MODE_SERIAL) && (PAGE(page_number) != PAGE(config.page))) ||
			((transmission_mode == TRANSMISSION_MODE_PARALLEL) && (PAGE(page_number) != PAGE(config.page)) && (m == MAGAZINE(config.page)))
			)) {
			receiving_data = NO;
			return nullptr;
		}

		// Page transmission is terminated, however now we are waiting for our new page
		if (page_number != config.page) return nullptr;

		// Now we have the begining of page transmission; if there is page_buffer pending, process it
		if (page_buffer.tainted == YES) {
			// it would be nice, if subtitle hides on previous video frame, so we contract 40 ms (1 frame @25 fps)
			page_buffer.hide_timestamp = timestamp - 40;
			pageOut = process_page(&page_buffer);
		}

		page_buffer.show_timestamp = timestamp;
		page_buffer.hide_timestamp = 0;
		memset(page_buffer.text, 0x00, sizeof(page_buffer.text));
		page_buffer.tainted = NO;
		receiving_data = YES;
		primary_charset.g0_x28 = UNDEF;

		uint8_t c = (primary_charset.g0_m29 != UNDEF) ? primary_charset.g0_m29 : charset;
		remap_g0_charset(c);

		/*
		// I know -- not needed; in subtitles we will never need disturbing teletext page status bar
		// displaying tv station name, current time etc.
		if (flag_suppress_header == NO) {
		for (uint8_t i = 14; i < 40; i++) page_buffer.text[y][i] = telx_to_ucs2(packet->data[i]);
		//page_buffer.tainted = YES;
		}
		*/
	}
	else if ((m == MAGAZINE(config.page)) && (y >= 1) && (y <= 23) && (receiving_data == YES)) {
		// ETS 300 706, chapter 9.4.1: Packets X/26 at presentation Levels 1.5, 2.5, 3.5 are used for addressing
		// a character location and overwriting the existing character defined on the Level 1 page
		// ETS 300 706, annex B.2.2: Packets with Y = 26 shall be transmitted before any packets with Y = 1 to Y = 25;
		// so page_buffer.text[y][i] may already contain any character received
		// in frame number 26, skip original G0 character
		for (uint8_t i = 0; i < 40; i++) if (page_buffer.text[y][i] == 0x00) page_buffer.text[y][i] = telx_to_ucs2(packet->data[i]);
		page_buffer.tainted = YES;
	}
	else if ((m == MAGAZINE(config.page)) && (y == 26) && (receiving_data == YES)) {
		// ETS 300 706, chapter 12.3.2: X/26 definition
		uint8_t x26_row = 0;
		uint8_t x26_col = 0;

		uint32_t triplets[13] = { 0 };
		for (uint8_t i = 1, j = 0; i < 40; i += 3, j++) triplets[j] = unham_24_18((packet->data[i + 2] << 16) | (packet->data[i + 1] << 8) | packet->data[i]);

		for (uint8_t j = 0; j < 13; j++) {
			if (triplets[j] == 0xffffffff) {
				// invalid data (HAM24/18 uncorrectable error detected), skip group
				VERBOSE_ONLY fprintf(stderr, "! Unrecoverable data error; UNHAM24/18()=%04x\n", triplets[j]);
				continue;
			}

			uint8_t data = (triplets[j] & 0x3f800) >> 11;
			uint8_t mode = (triplets[j] & 0x7c0) >> 6;
			uint8_t address = triplets[j] & 0x3f;
			uint8_t row_address_group = (address >= 40) && (address <= 63);

			// ETS 300 706, chapter 12.3.1, table 27: set active position
			if ((mode == 0x04) && (row_address_group == YES)) {
				x26_row = address - 40;
				if (x26_row == 0) x26_row = 24;
				x26_col = 0;
			}

			// ETS 300 706, chapter 12.3.1, table 27: termination marker
			if ((mode >= 0x11) && (mode <= 0x1f) && (row_address_group == YES)) break;

			// ETS 300 706, chapter 12.3.1, table 27: character from G2 set
			if ((mode == 0x0f) && (row_address_group == NO)) {
				x26_col = address;
				if (data > 31) page_buffer.text[x26_row][x26_col] = G2[0][data - 0x20];
			}

			// ETS 300 706, chapter 12.3.1, table 27: G0 character with diacritical mark
			if ((mode >= 0x11) && (mode <= 0x1f) && (row_address_group == NO)) {
				x26_col = address;

				// A - Z
				if ((data >= 65) && (data <= 90)) page_buffer.text[x26_row][x26_col] = G2_ACCENTS[mode - 0x11][data - 65];
				// a - z
				else if ((data >= 97) && (data <= 122)) page_buffer.text[x26_row][x26_col] = G2_ACCENTS[mode - 0x11][data - 71];
				// other
				else page_buffer.text[x26_row][x26_col] = telx_to_ucs2(data);
			}
		}
	}
	else if ((m == MAGAZINE(config.page)) && (y == 28) && (receiving_data == YES)) {
		// TODO:
		//   ETS 300 706, chapter 9.4.7: Packet X/28/4
		//   Where packets 28/0 and 28/4 are both transmitted as part of a page, packet 28/0 takes precedence over 28/4 for all but the colour map entry coding.
		if ((designation_code == 0) || (designation_code == 4)) {
			// ETS 300 706, chapter 9.4.2: Packet X/28/0 Format 1
			// ETS 300 706, chapter 9.4.7: Packet X/28/4
			uint32_t triplet0 = unham_24_18((packet->data[3] << 16) | (packet->data[2] << 8) | packet->data[1]);

			if (triplet0 == 0xffffffff) {
				// invalid data (HAM24/18 uncorrectable error detected), skip group
				VERBOSE_ONLY fprintf(stderr, "! Unrecoverable data error; UNHAM24/18()=%04x\n", triplet0);
			}
			else {
				// ETS 300 706, chapter 9.4.2: Packet X/28/0 Format 1 only
				if ((triplet0 & 0x0f) == 0x00) {
					primary_charset.g0_x28 = (triplet0 & 0x3f80) >> 7;
					remap_g0_charset(primary_charset.g0_x28);
				}
			}
		}
	}
	else if ((m == MAGAZINE(config.page)) && (y == 29)) {
		// TODO:
		//   ETS 300 706, chapter 9.5.1 Packet M/29/0
		//   Where M/29/0 and M/29/4 are transmitted for the same magazine, M/29/0 takes precedence over M/29/4.
		if ((designation_code == 0) || (designation_code == 4)) {
			// ETS 300 706, chapter 9.5.1: Packet M/29/0
			// ETS 300 706, chapter 9.5.3: Packet M/29/4
			uint32_t triplet0 = unham_24_18((packet->data[3] << 16) | (packet->data[2] << 8) | packet->data[1]);

			if (triplet0 == 0xffffffff) {
				// invalid data (HAM24/18 uncorrectable error detected), skip group
				VERBOSE_ONLY fprintf(stderr, "! Unrecoverable data error; UNHAM24/18()=%04x\n", triplet0);
			}
			else {
				// ETS 300 706, table 11: Coding of Packet M/29/0
				// ETS 300 706, table 13: Coding of Packet M/29/4
				if ((triplet0 & 0xff) == 0x00) {
					primary_charset.g0_m29 = (triplet0 & 0x3f80) >> 7;
					// X/28 takes precedence over M/29
					if (primary_charset.g0_x28 == UNDEF) {
						remap_g0_charset(primary_charset.g0_m29);
					}
				}
			}
		}
	}
	else if ((m == 8) && (y == 30)) {
		// ETS 300 706, chapter 9.8: Broadcast Service Data Packets
		if (states.programme_info_processed == NO) {
			// ETS 300 706, chapter 9.8.1: Packet 8/30 Format 1
			if (unham_8_4(packet->data[0]) < 2) {
				STDERR_ONLY fprintf(stderr, "- Programme Identification Data = ");
				for (uint8_t i = 20; i < 40; i++) {
					uint8_t c = (uint8_t)telx_to_ucs2(packet->data[i]);
					// strip any control codes from PID, eg. TVP station
					if (c < 0x20) continue;

					char u[4] = { 0, 0, 0, 0 };
					ucs2_to_utf8(u, c);
					STDERR_ONLY fprintf(stderr, "%s", u);
				}
				STDERR_ONLY fprintf(stderr, "\n");

				// OMG! ETS 300 706 stores timestamp in 7 bytes in Modified Julian Day in BCD format + HH:MM:SS in BCD format
				// + timezone as 5-bit count of half-hours from GMT with 1-bit sign
				// In addition all decimals are incremented by 1 before transmission.
				uint32_t t = 0;
				// 1st step: BCD to Modified Julian Day
				t += (packet->data[10] & 0x0f) * 10000;
				t += ((packet->data[11] & 0xf0) >> 4) * 1000;
				t += (packet->data[11] & 0x0f) * 100;
				t += ((packet->data[12] & 0xf0) >> 4) * 10;
				t += (packet->data[12] & 0x0f);
				t -= 11111;
				// 2nd step: conversion Modified Julian Day to unix timestamp
				t = (t - 40587) * 86400;
				// 3rd step: add time
				t += 3600 * (((packet->data[13] & 0xf0) >> 4) * 10 + (packet->data[13] & 0x0f));
				t += 60 * (((packet->data[14] & 0xf0) >> 4) * 10 + (packet->data[14] & 0x0f));
				t += (((packet->data[15] & 0xf0) >> 4) * 10 + (packet->data[15] & 0x0f));
				t -= 40271;
				// 4th step: conversion to time_t
				time_t t0 = (time_t)t;
				// ctime output itself is \n-ended
				STDERR_ONLY fprintf(stderr, "- Programme Timestamp (UTC) = %s", ctime(&t0));

				VERBOSE_ONLY fprintf(stderr, "- Transmission mode = %s\n", (transmission_mode == TRANSMISSION_MODE_SERIAL ? "serial" : "parallel"));

				if (config.se_mode == YES) {
					STDERR_ONLY fprintf(stderr, "- Broadcast Service Data Packet received, resetting UTC referential value to %s", ctime(&t0));
					config.utc_refvalue = t;
					states.pts_initialized = NO;
				}

				states.programme_info_processed = YES;
			}
		}
	}

	return pageOut;
}
}

TeletextToTTML::TeletextToTTML(unsigned pageNum, const std::string &lang, uint64_t splitDurationInMs, TimingPolicy timingPolicy)
: pageNum(pageNum), lang(lang), timingPolicy(timingPolicy), splitDurationIn180k(timescaleToClock(splitDurationInMs, 1000)) {
	addInput(new Input<DataAVPacket>(this));
	output = addOutput<OutputDataDefault<DataAVPacket>>();
}

void TeletextToTTML::sendSample(const std::string &sample) {
	auto out = output->getBuffer(0);
	out->setMediaTime(intClock);
	auto pkt = out->getPacket();
	pkt->size = (int)sample.size();
	pkt->data = (uint8_t*)av_malloc(pkt->size);
	pkt->flags |= AV_PKT_FLAG_KEY;
	memcpy(pkt->data, (uint8_t*)sample.c_str(), pkt->size);
	output->emit(out);
}

void TeletextToTTML::process(Data data) {
	if (!firstDataAbsTimeInMs)
		firstDataAbsTimeInMs = data->getAbsTime(1000);
	//TODO
	//14. add flush() for ondemand samples
	//15. UTF8 to TTML formatting? accent + EOLs </br>
	auto sub = safe_cast<const DataAVPacket>(data);
	output->setMetadata(data->getMetadata());
	if (!sub->size()) { //on sparse stream, we may be regularly awaken: generate samples when needed
		extClock = sub->getMediaTime();
		const int64_t prevSplit = (intClock / splitDurationIn180k) * splitDurationIn180k;
		const int64_t nextSplit = prevSplit + splitDurationIn180k;
		if ((int64_t)(extClock - delayIn180k) > nextSplit) {
			sendSample(toTTML(clockToTimescale(prevSplit, 1000), clockToTimescale(nextSplit, 1000)));
			intClock = nextSplit;
		}
		return;
	}

	auto pkt = sub->getPacket();
	int pes_packet_length = pkt->size;
	config.page = pageNum;
	int i = 1;
	while (i <= pes_packet_length - 6) {
		auto data_unit_id = (data_unit_t)pkt->data[i++];
		uint8_t data_unit_len = pkt->data[i++];
		if ((data_unit_id == DATA_UNIT_EBU_TELETEXT_NONSUBTITLE) || (data_unit_id == DATA_UNIT_EBU_TELETEXT_SUBTITLE)) {
			// teletext payload has always size 44 bytes
			if (data_unit_len == 44) {
				uint8_t txdata[44];
				// reverse endianess (via lookup table), ETS 300 706, chapter 7.1
				for (uint8_t j = 0; j < data_unit_len; j++) {
					txdata[j] = REVERSE_8[pkt->data[i + j]];
				}

				// FIXME: This explicit type conversion could be a problem some day -- do not need to be platform independant
				auto page = process_telx_packet(data_unit_id, (teletext_packet_payload_t*)txdata, pkt->pts);
				if (page) {
					auto const codecCtx = safe_cast<const MetadataPktLibav>(data->getMetadata())->getAVCodecContext();
					log(Debug, "frames_produced %s, show=%s, hide=%s", page->frames_produced, convertToTimescale(page->show_timestamp * codecCtx->pkt_timebase.num, codecCtx->pkt_timebase.den, 1000), convertToTimescale(page->hide_timestamp * codecCtx->pkt_timebase.num, codecCtx->pkt_timebase.den, 1000));
					if (data->getMediaTime() < intClock) {
						log(Warning, "Timing error: received %s but internal clock is already at %s", data->getMediaTime(), intClock);
					}

					auto const startTimeInMs = std::max<int64_t>(convertToTimescale(pkt->pts * codecCtx->pkt_timebase.num, codecCtx->pkt_timebase.den, 1000), convertToTimescale(page->show_timestamp * codecCtx->pkt_timebase.num, codecCtx->pkt_timebase.den, 1000));
					auto const durationInMs = convertToTimescale((page->hide_timestamp - page->show_timestamp) * codecCtx->pkt_timebase.num, codecCtx->pkt_timebase.den, 1000);
					page->startTimeInMs = startTimeInMs;
					page->endTimeInMs = startTimeInMs + durationInMs;
					currentPages.push_back(std::move(page));
				}
			}
		}

		i += data_unit_len;
	}
}

}
}
