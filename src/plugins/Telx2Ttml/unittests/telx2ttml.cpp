#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_media/common/metadata.hpp"
#include "../telx2ttml.hpp"
#include "lib_utils/log_sink.hpp"
#include "lib_utils/log.hpp" // g_Log
#include <string.h>

using std::make_shared;
using namespace Tests;
using namespace Modules;

namespace {
std::shared_ptr<DataBase> createPacket(SpanC bytes) {
	auto pkt = make_shared<DataRaw>(bytes.len);
	memcpy(pkt->buffer->data().ptr, bytes.ptr, bytes.len);
	return pkt;
}

std::shared_ptr<DataBase> getTeletextTestFrame() {
	static const uint8_t teletext[] = {
		// garbage data
		0xde, 0x03, 0x2c, 0x03, 0xde, 0x07, 0x55, 0x00, 0x00,
	};

	auto r = createPacket(teletext);
	r->setMetadata(make_shared<MetadataPkt>(SUBTITLE_PKT));
	return r;
}
}
unittest("telx2ttml: simple") {
	struct ScopedNullLogger : private LogSink {
			ScopedNullLogger() : oldLog(g_Log) {
				g_Log = this;
			}

			~ScopedNullLogger() {
				g_Log = oldLog;
			}

			void log(Level, const char*) {}

		private:
			LogSink* const oldLog;
	};

	ScopedNullLogger silence_logging;

	TeletextToTtmlConfig cfg;
	auto reader = loadModule("TeletextToTTML", &NullHost, &cfg);
	reader->getInput(0)->push(getTeletextTestFrame());
	reader->process();
}

