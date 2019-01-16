#include "video_generator.hpp"
#include "../common/metadata.hpp"
#include <string.h> // memset
#include <cassert>

namespace {

span<const char*> getFont();

void renderChar(uint8_t* dst, size_t stride, int asciiValue) {
	auto font = getFont();

	assert(asciiValue >=0 && asciiValue <= (int)font.len);

	auto src = font[asciiValue];

	for(int y=0; y < 5; ++y) {
		for(int x=0; x < 5; ++x) {
			if(src[x] == 'X')
				dst[x] = 0xFF;
		}
		src += 5;
		dst += stride;
	}
}

void renderString(uint8_t* dst, size_t stride, const char* buffer) {
	auto const hstep = 6;
	int i=0;
	while(buffer[i]) {
		renderChar(dst, stride, buffer[i]);
		dst += hstep;
		++i;
	}
}

void renderNumber(uint8_t* dst, size_t stride, int value) {
	char buffer[256];
	sprintf(buffer, "%03d", value);
	renderString(dst, stride, buffer);
}

}

namespace Modules {
namespace In {

VideoGenerator::VideoGenerator(KHost* host, int maxFrames_, int frameRate)
	:  m_host(host), maxFrames(maxFrames_), m_frameRate(frameRate) {
	output = addOutput<OutputPicture>();
	output->setMetadata(make_shared<MetadataRawVideo>());
	m_host->activate(true);
}

void VideoGenerator::process() {

	if(maxFrames && m_numFrames >= (uint64_t)maxFrames) {
		m_host->activate(false);
		return;
	}

	auto const dim = Resolution(320, 180);
	auto pic = DataPicture::create(output, dim, PixelFormat::I420);

	// generate video
	auto const p = pic->data().ptr;
	auto const flash = (m_numFrames % m_frameRate) == 0;
	auto const val = flash ? 0xCC : 0x80;
	memset(p, val, pic->getSize());

	if(dim.width > 32 && dim.height > 32) {

		auto dst = pic->getPlane(0);
		auto stride = pic->getStride(0);
		auto const margin = 3;
		dst += margin;
		dst += stride * margin;

		renderNumber(dst, stride, m_numFrames);
	}

	if(dim.width > 64 && dim.height > 32) {

		auto dst = pic->getPlane(0);
		auto stride = pic->getStride(0);
		auto const h_margin = (dim.width - 6 * 7) / 2;
		auto const v_margin = dim.height/ 2;
		dst += h_margin;
		dst += stride * v_margin;

		renderString(dst, stride, "SIGNALS");
	}

	auto const framePeriodIn180k = IClock::Rate / m_frameRate;
	assert(IClock::Rate % m_frameRate == 0);
	pic->setMediaTime(m_numFrames * framePeriodIn180k);

	output->post(pic);

	++m_numFrames;
}

}
}

namespace {
span<const char*> getFont() {
	static const char* font[] = {
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",

		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",

		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",

		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",

		// space
		".........................",

		// bang
		"..X.."
		"..X.."
		"..X.."
		"....."
		"..X..",

		// double-quote
		".X.X."
		".X.X."
		"....."
		"....."
		".....",

		// hash
		".X.X."
		"XXXXX"
		".X.X."
		"XXXXX"
		".X.X.",

		// dollar
		".XXX."
		"X.X.."
		".XXX."
		"..X.X"
		".XXX.",

		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",

		"....X"
		"...X."
		"..X.."
		".X..."
		"X....",

		".XXX."
		"X...X"
		"X...X"
		"X...X"
		".XXX.",

		"....X"
		"...XX"
		"..X.X"
		"....X"
		"....X",

		".XXX."
		"X...X"
		"..XX."
		".X..."
		"XXXXX",

		".XXX."
		"X...X"
		"..XX."
		"X...X"
		".XXX.",

		"X...X"
		"X...X"
		".XXXX"
		"....X"
		"....X",

		"XXXXX"
		"X...."
		"XXXX."
		"....X"
		"XXXX.",

		".XXX."
		"X...."
		"XXXX."
		"X...X"
		".XXX.",

		"XXXXX"
		"....X"
		"....X"
		"....X"
		"....X",

		".XXX."
		"X...X"
		".XXX."
		"X...X"
		".XXX.",

		".XXX."
		"X...X"
		".XXXX"
		"....X"
		".XXX.",

		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",

		// @
		".XXX."
		"X.X.X"
		"X.XXX"
		"X...."
		".XXX.",

		".XXX."
		"X...X"
		"XXXXX"
		"X...X"
		"X...X",

		"XXXX."
		"X...X"
		"XXXX."
		"X...X"
		"XXXXX",

		".XXXX"
		"X...."
		"X...."
		"X...."
		".XXXX",

		"XXXX."
		"X...X"
		"X...X"
		"X...X"
		"XXXX.",

		"XXXXX"
		"X...."
		"XXX.."
		"X...."
		"XXXXX",

		"XXXXX"
		"X...."
		"XXX.."
		"X...."
		"X....",

		".XXXX"
		"X...."
		"X..XX"
		"X...X"
		".XXXX",

		"X...X"
		"X...X"
		"XXXXX"
		"X...X"
		"X...X",

		"XXXXX"
		"..X.."
		"..X.."
		"..X.."
		"XXXXX",

		"XXXXX"
		"...X."
		"...X."
		"...X."
		"XXX..",

		"X..X."
		"X.X.."
		"XX..."
		"X.X.."
		"X..X.",

		"X...."
		"X...."
		"X...."
		"X...."
		"XXXXX",

		"X...X"
		"XX.XX"
		"X.X.X"
		"X...X"
		"X...X",

		"X...X"
		"XX..X"
		"X.X.X"
		"X..XX"
		"X...X",

		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",
		"XXXXXXXXXXXXXXXXXXXXXXXXX",

		"XXXXX"
		"X...."
		".XXX."
		"....X"
		"XXXXX",

	};

	return font;
}
}
