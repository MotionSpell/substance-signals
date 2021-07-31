#include "lib_modules/utils/factory.hpp"
#include "../common/http_sender.hpp"
#include "lib_utils/tools.hpp" // enforce
#include <string.h> // memcpy

using namespace Modules;

namespace {

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

void skipBox(span<const uint8_t>& bs, uint32_t boxName) {
	auto size = readU32BE(bs);
	auto type = readU32BE(bs);
	if (boxName && type != boxName)
		throw std::runtime_error("skipBox: unexpected box");
	bs += size - 8;
}

struct MS_HSS : public ModuleS {
		MS_HSS(KHost* host, const std::string &url)
			: m_host(host) {
			m_httpSender = createHttpSender({url, "MS-HSS", POST, {}}, m_host);
		}

		~MS_HSS() {
			// tell the remote application to close the session
			std::vector<uint8_t> endOfSessionSuffix = { 0, 0, 0, 8, 'm', 'f', 'r', 'a' };
			if (m_httpSender) m_httpSender->send({endOfSessionSuffix.data(), endOfSessionSuffix.size()});
		}


		void processOne(Data data) override {

			// split 'data' into 'prefix' (ftyp/moov/etc.) and 'bs' (mdat)
			auto bs = data->data();

			skipBox(bs, FOURCC("ftyp"));
			skipBox(bs, 0);
			skipBox(bs, FOURCC("free"));
			skipBox(bs, FOURCC("moov"));

			{
				auto prefix = data->data();
				prefix.len = bs.ptr - prefix.ptr;
				if (m_httpSender) m_httpSender->setPrefix(prefix);
			}

			if (m_httpSender) m_httpSender->send(bs);
		}

		void flush() override {
			if (m_httpSender) m_httpSender->send({});
		}

	private:
		std::unique_ptr<HttpSender> m_httpSender;
		KHost* const m_host;
};

IModule* createObject(KHost* host, void* va) {
	auto config = (std::string*)va;
	enforce(host, "MS_HSS: host can't be NULL");
	enforce(config, "MS_HSS: config can't be NULL");
	return createModule<MS_HSS>(host, *config).release();
}

auto const registered = Factory::registerModule("MS_HSS", &createObject);
}
