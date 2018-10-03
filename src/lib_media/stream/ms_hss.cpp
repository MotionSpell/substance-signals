#include "ms_hss.hpp"
#include "lib_utils/log_sink.hpp" // Warning
#include "lib_utils/format.hpp"

extern "C" {
#include <gpac/bitstream.h>
}

#define U32LE(p) (((((u8*)p)[0]) << 24) | ((((u8*)p)[1]) << 16) | ((((u8*)p)[2]) << 8) | (((u8*)p)[3]))

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

void MS_HSS::newFileCallback(void *ptr) {
	auto const datac = (char*)ptr;
	auto read = gf_bs_read_data(curTransferedBs, datac, 8);
	if (read != 8)
		throw error("I/O error (1)");
	u32 size = U32LE(ptr);
	u32 type = U32LE(ptr + 4);
	if (type != FOURCC("ftyp"))
		throw error("ftyp not found");
	read = gf_bs_read_data(curTransferedBs, datac, size - 8);
	if (read != size - 8)
		throw error("I/O error (2)");

	read = gf_bs_read_data(curTransferedBs, datac, 8);
	if (read != 8)
		throw error("I/O error (3)");
	size = U32LE(ptr);
	read = gf_bs_read_data(curTransferedBs, datac, size - 8);
	if (read != size - 8)
		throw error("I/O error (4)");

	read = gf_bs_read_data(curTransferedBs, datac, 8);
	if (read != 8)
		throw error("I/O error (5)");
	size = U32LE(ptr);
	type = U32LE(ptr + 4);
	if (type != FOURCC("free"))
		throw error("free not found");
	read = gf_bs_read_data(curTransferedBs, datac, size - 8);
	if (read != size - 8)
		throw error("I/O error (6)");

	read = gf_bs_read_data(curTransferedBs, datac, 8);
	if (read != 8)
		throw error("I/O error (7)");
	size = U32LE(ptr);
	type = U32LE(ptr + 4);
	if (type != FOURCC("moov"))
		throw error("moov not found");
	read = gf_bs_read_data(curTransferedBs, datac, size - 8);
	if (read != size - 8)
		throw error("I/O error (8)");
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
