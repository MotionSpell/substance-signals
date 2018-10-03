#include "ms_hss.hpp"
#include "lib_utils/log_sink.hpp" // Warning
#include "lib_utils/format.hpp"

extern "C" {
#include <gpac/bitstream.h>
}

inline uint32_t U32LE(uint8_t* p) {
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | (p[3] << 0);
}

template<size_t N>
constexpr uint32_t FOURCC(const char (&a)[N]) {
	static_assert(N == 5, "FOURCC must be composed of 4 characters");
	return (a[0]<<24) | (a[1]<<16) | (a[2]<<8) | a[3];
}

namespace Modules {
namespace Stream {

MS_HSS::MS_HSS(IModuleHost* host, const std::string &url)
	: HTTP(host, HttpOutputConfig{url}), m_host(host) {
}

void MS_HSS::newFileCallback(span<uint8_t> out) {
	auto buf = out.ptr;

	// skip 'ftyp' box
	readTransferedBs(buf, 8);
	u32 size = U32LE(buf + 0);
	u32 type = U32LE(buf + 4);
	if (type != FOURCC("ftyp"))
		throw error("ftyp not found");
	readTransferedBs(buf, size - 8);

	// skip some box
	readTransferedBs(buf, 8);
	size = U32LE(buf);
	readTransferedBs(buf, size - 8);

	// skip 'free' box
	readTransferedBs(buf, 8);
	size = U32LE(buf + 0);
	type = U32LE(buf + 4);
	if (type != FOURCC("free"))
		throw error("free not found");
	readTransferedBs(buf, size - 8);

	// put the contents of 'moov' box into 'buf'
	readTransferedBs(buf, 8);
	size = U32LE(buf + 0);
	type = U32LE(buf + 4);
	if (type != FOURCC("moov"))
		throw error("moov not found");
	readTransferedBs(buf, size - 8);
}

size_t MS_HSS::endOfSession(span<uint8_t> buffer) {
	auto const mfraSize = 8;
	if (buffer.len < mfraSize) {
		m_host->log(Warning, format( "endOfSession: needed to write %s bytes but buffer size is %s.", mfraSize, buffer.len).c_str());
		return 0;
	}
	auto bs = gf_bs_new((const char*)buffer.ptr, buffer.len, GF_BITSTREAM_WRITE);
	gf_bs_write_u32(bs, mfraSize); //size (Box)
	gf_bs_write_u32(bs, FOURCC("mfra"));
	assert(gf_bs_get_position(bs) == mfraSize);
	gf_bs_del(bs);

	return mfraSize;
}

}
}
