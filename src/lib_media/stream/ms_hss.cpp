#include "ms_hss.hpp"
#include "lib_utils/log_sink.hpp" // Warning
#include "lib_utils/format.hpp"

inline uint32_t U32BE(uint8_t* p) {
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | (p[3] << 0);
}

inline void Write_U32BE(uint8_t*& p, uint32_t val) {
	p[0] = (val >> 24) & 0xFF;
	p[1] = (val >> 16) & 0xFF;
	p[2] = (val >> 8) & 0xFF;
	p[3] = (val >> 0) & 0xFF;
	p+=4;
}

template<size_t N>
constexpr uint32_t FOURCC(const char (&a)[N]) {
	static_assert(N == 5, "FOURCC must be composed of 4 characters");
	return (a[0]<<24) | (a[1]<<16) | (a[2]<<8) | a[3];
}

namespace Modules {
namespace Stream {

MS_HSS::MS_HSS(IModuleHost* host, const std::string &url)
	: m_http(make_unique<Out::HTTP>(host, HttpOutputConfig{url})), m_host(host) {
	m_http->m_controller = this;
}

void MS_HSS::newFileCallback(span<uint8_t> out) {
	auto buf = out.ptr;

	// skip 'ftyp' box
	m_http->readTransferedBs(buf, 8);
	auto size = U32BE(buf + 0);
	auto type = U32BE(buf + 4);
	if (type != FOURCC("ftyp"))
		throw error("ftyp not found");
	m_http->readTransferedBs(buf, size - 8);

	// skip some box
	m_http->readTransferedBs(buf, 8);
	size = U32BE(buf);
	m_http->readTransferedBs(buf, size - 8);

	// skip 'free' box
	m_http->readTransferedBs(buf, 8);
	size = U32BE(buf + 0);
	type = U32BE(buf + 4);
	if (type != FOURCC("free"))
		throw error("free not found");
	m_http->readTransferedBs(buf, size - 8);

	// put the contents of 'moov' box into 'buf'
	m_http->readTransferedBs(buf, 8);
	size = U32BE(buf + 0);
	type = U32BE(buf + 4);
	if (type != FOURCC("moov"))
		throw error("moov not found");
	m_http->readTransferedBs(buf, size - 8);
}

size_t MS_HSS::endOfSession(span<uint8_t> buffer) {
	auto const mfraSize = 8;

	if (buffer.len < mfraSize) {
		m_host->log(Warning, format( "endOfSession: needed to write %s bytes but buffer size is %s.", mfraSize, buffer.len).c_str());
		return 0;
	}

	auto bs = buffer.ptr;
	Write_U32BE(bs, mfraSize); //size (Box)
	Write_U32BE(bs, FOURCC("mfra"));

	return mfraSize;
}

}
}
