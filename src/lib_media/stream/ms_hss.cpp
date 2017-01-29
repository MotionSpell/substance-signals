#include "ms_hss.hpp"

extern "C" {
#include <gpac/bitstream.h>
}

#define U32LE(p) (((((u8*)p)[0]) << 24) | ((((u8*)p)[1]) << 16) | ((((u8*)p)[2]) << 8) | (((u8*)p)[3]))

namespace Modules {
namespace Stream {

MS_HSS::MS_HSS(const std::string &url) : HTTP(url) {
}

void MS_HSS::newFileCallback(void *ptr) {
	auto const datac = (char*)ptr;
	auto read = gf_bs_read_data(curTransferedBs, datac, 8);
	if (read != 8)
		throw error("I/O error (1)");
	u32 size = U32LE(ptr);
	u32 type = U32LE(ptr + 4);
	if (type != GF_4CC('f', 't', 'y', 'p'))
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
	if (type != GF_4CC('f', 'r', 'e', 'e'))
		throw error("free not found");
	read = gf_bs_read_data(curTransferedBs, datac, size - 8);
	if (read != size - 8)
		throw error("I/O error (6)");

	read = gf_bs_read_data(curTransferedBs, datac, 8);
	if (read != 8)
		throw error("I/O error (7)");
	size = U32LE(ptr);
	type = U32LE(ptr + 4);
	if (type != GF_4CC('m', 'o', 'o', 'v'))
		throw error("moov not found");
	read = gf_bs_read_data(curTransferedBs, datac, size - 8);
	if (read != size - 8)
		throw error("I/O error (8)");
}

}
}
