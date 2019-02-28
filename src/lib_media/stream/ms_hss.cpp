#include "ms_hss.hpp"
#include <string.h> // memcpy

inline uint32_t readU32BE(span<const uint8_t>& p) {
	auto val = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | (p[3] << 0);
	p += 4;
	return val;
}

template<size_t N>
constexpr uint32_t FOURCC(const char (&a)[N]) {
	static_assert(N == 5, "FOURCC must be composed of 4 characters");
	return (a[0]<<24) | (a[1]<<16) | (a[2]<<8) | a[3];
}

namespace Modules {
namespace Stream {

static
void skipBox(span<const uint8_t>& bs, uint32_t boxName) {
	auto size = readU32BE(bs);
	auto type = readU32BE(bs);
	if (boxName && type != boxName)
		throw std::runtime_error("skipBox: unexpected box");
	bs += size - 8;
}

MS_HSS::MS_HSS(KHost* host, const std::string &url)
	: m_host(host) {
	m_httpSender = createHttpSender(url, "MS-HSS", false, {}, m_host);
}

MS_HSS::~MS_HSS() {
	// tell the remote application to close the session
	std::vector<uint8_t> endOfSessionSuffix = { 0, 0, 0, 8, 'm', 'f', 'r', 'a' };
	m_httpSender->send({endOfSessionSuffix.data(), endOfSessionSuffix.size()});
}


void MS_HSS::processOne(Data data) {

	// split 'data' into 'prefix' (ftyp/moov/etc.) and 'bs' (mdat)
	auto bs = data->data();

	skipBox(bs, FOURCC("ftyp"));
	skipBox(bs, 0);
	skipBox(bs, FOURCC("free"));
	skipBox(bs, FOURCC("moov"));

	{
		auto prefix = data->data();
		prefix.len = bs.ptr - prefix.ptr;
		m_httpSender->setPrefix(prefix);
	}

	m_httpSender->send(bs);
}

void MS_HSS::flush() {
	m_httpSender->send({});
}

}
}
