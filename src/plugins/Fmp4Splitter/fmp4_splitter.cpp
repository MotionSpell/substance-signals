#include <cassert>
#include "lib_utils/log_sink.hpp"
#include "lib_utils/tools.hpp" // enforce
#include "lib_modules/utils/helper.hpp"
#include "lib_modules/utils/factory.hpp"

using namespace Modules;

namespace Modules {

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
		std::vector<uint8_t> currBox;

		void pushByte(uint8_t byte) {
			currBox.push_back(byte);

			if(insideHeader) {
				if(headerBytes < 4) {
					boxBytes <<= 8;
					boxBytes |= byte;
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
					// flush current box
					auto out = output->allocData<DataRaw>(currBox.size());
					memcpy(out->buffer->data().ptr, currBox.data(), currBox.size());
					output->post(out);
					currBox.clear();

					// go back to 'header' state
					boxBytes = 0;
					insideHeader = true;
				}
			}
		}

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

