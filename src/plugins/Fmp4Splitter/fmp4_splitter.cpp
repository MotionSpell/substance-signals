#include <cassert>
#include "lib_utils/log_sink.hpp"
#include "lib_utils/tools.hpp" // enforce
#include "lib_modules/utils/helper.hpp"
#include "lib_modules/utils/factory.hpp"

using namespace Modules;

namespace Modules {

template<size_t N>
constexpr uint32_t FOURCC(const char (&a)[N]) {
	static_assert(N == 5, "FOURCC must be composed of 4 characters");
	return (a[0]<<24) | (a[1]<<16) | (a[2]<<8) | a[3];
}

// reconstruct framing of top-level MP4 boxes
class Fmp4Splitter : public ModuleS {
	public:
		Fmp4Splitter(KHost* host)
			: m_host(host) {
			output = addOutput();
		}

		void processOne(Data data) override {
			for(auto byte : data->data())
				pushByte(byte);
		}

	private:
		KHost* const m_host;
		OutputDefault* output;
		std::vector<uint8_t> currData; // box buffer. Can contain multiple concatenated boxes.

		void pushByte(uint8_t byte) {
			currData.push_back(byte);

			if(insideHeader) {
				if(headerBytes < 4) {
					boxBytes <<= 8;
					boxBytes |= byte;
				} else {
					lastFourcc <<= 8;
					lastFourcc |= byte;
				}
				// reading header
				headerBytes ++;
				assert(headerBytes <= 8);
				if(headerBytes == 8) {
					if(boxBytes > 8) {
						boxBytes -= 8;
						insideHeader = false;
					}

					headerBytes = 0;
				}
			} else {
				assert(boxBytes > 0);
				boxBytes --;

				// is the current box complete?
				if(boxBytes == 0) {

					if(lastFourcc == FOURCC("mdat")) {
						// flush current box
						auto out = output->allocData<DataRaw>(currData.size());
						memcpy(out->buffer->data().ptr, currData.data(), currData.size());
						output->post(out);
						currData.clear();
					}

					// go back to 'header' state
					boxBytes = 0;
					insideHeader = true;
				}
			}
		}

		uint32_t lastFourcc = 0;
		int insideHeader = true;
		int headerBytes = 0;
		int64_t boxBytes = 0;
};

}

namespace {
IModule* createObject(KHost* host, void* va) {
	(void)va;
	enforce(host, "Fmp4Splitter: host can't be NULL");
	return new ModuleDefault<Fmp4Splitter>(256, host);
}

auto const registered = Factory::registerModule("Fmp4Splitter", &createObject);
}

