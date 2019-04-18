#include "telx2ttml.hpp"
#include "lib_media/common/attributes.hpp"
#include "lib_modules/utils/factory.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_utils/log_sink.hpp" // Warning
#include "lib_utils/time.hpp" // timeInMsToStr
#include "lib_utils/tools.hpp" // enforce
#include <algorithm> // std::max
#include <vector>
#include <sstream>
#include <cassert>

extern "C" {
#include <libavcodec/avcodec.h> // AVCodecContext
}

auto const DEBUG_DISPLAY_TIMESTAMPS = false;

static std::string timecodeToString(int64_t timeInMs) {
	const size_t timecodeSize = 24;
	char timecode[timecodeSize] = { 0 };
	timeInMsToStr(timeInMs, timecode, ".");
	timecode[timecodeSize - 1] = 0;
	return timecode;
}

struct Page {
	Page() {
		lines.push_back({});
	}

	std::string toString() const {
		std::string r;

		for(auto& ss : lines)
			r += ss + "\n";

		return r;
	}

	std::string toTTML(int64_t startTimeInMs, int64_t endTimeInMs, int64_t idx) const {
		assert(!lines.empty());

		auto const timecodeShow = timecodeToString(startTimeInMs);
		auto const timecodeHide = timecodeToString(endTimeInMs);

		std::stringstream ttml;
		ttml << "      <p region=\"Region\" style=\"textAlignment_0\" begin=\"" << timecodeShow << "\" end=\"" << timecodeHide << "\" xml:id=\"s" << idx << "\">\n";

		if(DEBUG_DISPLAY_TIMESTAMPS) {
			ttml << "        <span style=\"Style0_0\">" << timecodeShow << " - " << timecodeHide << "</span>\n";
		} else {
			ttml << "        <span style=\"Style0_0\">";

			bool first = true;
			for(auto& line : lines) {
				if(line.empty())
					continue;

				if(!first)
					ttml << "<br/>\r\n";

				ttml << line;
				first = false;
			}

			ttml << "</span>\n";
		}

		ttml << "      </p>\n";

		return ttml.str();
	}

	int64_t tsInMs = 0, startTimeInMs = 0, endTimeInMs = 0, showTimestamp = 0, hideTimestamp = 0;
	std::vector<std::string> lines;
};

#include "telx.hpp" // requires 'Page' definition

using namespace Modules;

namespace {
class TeletextToTTML : public ModuleS {
	public:
		TeletextToTTML(KHost* host, TeletextToTtmlConfig* cfg)
			: m_host(host),
			  m_utcStartTime(cfg->utcStartTime),
			  pageNum(cfg->pageNum), lang(cfg->lang), timingPolicy(cfg->timingPolicy), maxPageDurIn180k(timescaleToClock(cfg->maxDelayBeforeEmptyInMs, 1000)), splitDurationIn180k(timescaleToClock(cfg->splitDurationInMs, 1000)) {
			config.host = host;
			enforce(cfg->utcStartTime != nullptr, "TeletextToTTML: utcStartTime can't be NULL");
			output = addOutput();
		}

		void processOne(Data data) override {
			if(inputs[0]->updateMetadata(data))
				output->setMetadata(data->getMetadata());

			extClock = data->getMediaTime();
			// TODO
			// 14. add flush() for ondemand samples
			// 15. UTF8 to TTML formatting? accent
			processTelx(data);
			dispatch();
		}

	private:
		KHost* const m_host;
		IUtcStartTimeQuery* const m_utcStartTime;
		OutputDefault* output;
		const unsigned pageNum;
		const std::string lang;
		const TeletextToTtmlConfig::TimingPolicy timingPolicy;
		int64_t intClock = 0, extClock = 0;
		const int64_t maxPageDurIn180k, splitDurationIn180k;
		std::vector<Page> currentPages;
		TeletextState config;

		std::string toTTML(int64_t startTimeInMs, int64_t endTimeInMs) {
			std::stringstream ttml;
			ttml << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n";
			ttml << "<tt xmlns=\"http://www.w3.org/ns/ttml\" xmlns:tt=\"http://www.w3.org/ns/ttml\" xmlns:ttm=\"http://www.w3.org/ns/ttml#metadata\" xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" xmlns:ttp=\"http://www.w3.org/ns/ttml#parameter\" xmlns:ebutts=\"urn:ebu:tt:style\" xmlns:ebuttm=\"urn:ebu:tt:metadata\" xml:lang=\"" << lang << "\" ttp:timeBase=\"media\">\n";
			ttml << "  <head>\n";
			ttml << "    <metadata>\n";
			ttml << "      <ebuttm:documentMetadata>\n";
			ttml << "        <ebuttm:conformsToStandard>urn:ebu:tt:distribution:2014-01</ebuttm:conformsToStandard>\n";
			ttml << "      </ebuttm:documentMetadata>\n";
			ttml << "    </metadata>\n";
			ttml << "    <styling>\n";
			ttml << "      <style xml:id=\"Style0_0\" tts:fontFamily=\"proportionalSansSerif\" tts:backgroundColor=\"#00000099\" tts:color=\"#FFFFFF\" tts:fontSize=\"100%\" tts:lineHeight=\"normal\" ebutts:linePadding=\"0.5c\" />\n";
			ttml << "      <style xml:id=\"textAlignment_0\" tts:textAlign=\"center\" />\n";
			ttml << "    </styling>\n";
			ttml << "    <layout>\n";
			ttml << "      <region xml:id=\"Region\" tts:origin=\"10% 10%\" tts:extent=\"80% 80%\" tts:displayAlign=\"after\" />\n";
			ttml << "    </layout>\n";
			ttml << "  </head>\n";
			ttml << "  <body>\n";
			ttml << "    <div>\n";

			int64_t offsetInMs;
			switch(timingPolicy) {
			case TeletextToTtmlConfig::AbsoluteUTC:
				offsetInMs = clockToTimescale(m_utcStartTime->query(), 1000);
				break;
			case TeletextToTtmlConfig::RelativeToMedia:
				offsetInMs = 0;
				break;
			case TeletextToTtmlConfig::RelativeToSplit:
				offsetInMs = -1 * startTimeInMs;
				break;
			default: throw error("Unknown timing policy (1)");
			}

			if(DEBUG_DISPLAY_TIMESTAMPS) {
				auto pageOut = make_unique<Page>();
				ttml << pageOut->toTTML(offsetInMs + startTimeInMs, offsetInMs + endTimeInMs, startTimeInMs / clockToTimescale(this->splitDurationIn180k, 1000));
			} else {
				for(auto& page : currentPages) {
					if(page.endTimeInMs > startTimeInMs && page.startTimeInMs < endTimeInMs) {
						auto localStartTimeInMs = std::max<int64_t>(page.startTimeInMs, startTimeInMs);
						auto localEndTimeInMs = std::min<int64_t>(page.endTimeInMs, endTimeInMs);
						m_host->log(Debug, format("[%s-%s]: %s - %s: %s", startTimeInMs, endTimeInMs, localStartTimeInMs, localEndTimeInMs, page.toString()).c_str());
						ttml << page.toTTML(localStartTimeInMs + offsetInMs, localEndTimeInMs + offsetInMs, startTimeInMs / clockToTimescale(this->splitDurationIn180k, 1000));
					}
				}
			}

			// remove outdated pages
			{
				auto page = currentPages.begin();
				while(page != currentPages.end()) {
					if(page->endTimeInMs <= endTimeInMs) {
						page = currentPages.erase(page);
					} else {
						++page;
					}
				}
			}

			ttml << "    </div>\n";
			ttml << "  </body>\n";
			ttml << "</tt>\n\n";
			return ttml.str();
		}

		void sendSample(const std::string& sample) {
			auto out = output->allocData<DataRaw>(0);
			out->setMediaTime(intClock);
			out->buffer->resize(sample.size());

			CueFlags flags {};
			flags.keyframe = true;
			out->set(flags);

			memcpy(out->buffer->data().ptr, (uint8_t*)sample.c_str(), sample.size());
			output->post(out);
		}

		void dispatch() {
			int64_t prevSplit = (intClock / splitDurationIn180k) * splitDurationIn180k;
			int64_t nextSplit = prevSplit + splitDurationIn180k;

			while(extClock - maxPageDurIn180k > nextSplit) {
				sendSample(toTTML(clockToTimescale(prevSplit, 1000), clockToTimescale(nextSplit, 1000)));
				intClock = nextSplit;
				prevSplit = (intClock / splitDurationIn180k) * splitDurationIn180k;
				nextSplit = prevSplit + splitDurationIn180k;
			}
		}

		void processTelx(Data sub) {
			auto data = sub->data();
			auto& cfg = config;
			cfg.page = pageNum;
			int i = 1;

			while(i <= int(data.len) - 6) {
				auto const dataUnitId = (DataUnit)data[i++];
				auto const dataUnitSize = data[i++];

				const uint8_t TELX_PAYLOAD_SIZE = 44;

				if(((dataUnitId == NonSubtitle) || (dataUnitId == Subtitle))
				    && (dataUnitSize == TELX_PAYLOAD_SIZE)) {

					if(i + TELX_PAYLOAD_SIZE > (int)data.len) {
						m_host->log(Warning, "truncated data unit");
						return;
					}

					uint8_t entitiesData[TELX_PAYLOAD_SIZE];
					for(int j = 0; j < TELX_PAYLOAD_SIZE; j++) {
						auto byte = data[i + j];
						entitiesData[j] = Reverse8[byte]; // reverse endianess
					}

					auto page = process_telx_packet(cfg, dataUnitId, (Payload*)entitiesData, sub->getMediaTime());

					if(page) {
						m_host->log(Debug,
						    format("show=%s:hide=%s, clocks:data=%s:int=%s,ext=%s, content=%s",
						        clockToTimescale(page->showTimestamp, 1000), clockToTimescale(page->hideTimestamp, 1000),
						        clockToTimescale(sub->getMediaTime(), 1000), clockToTimescale(intClock, 1000), clockToTimescale(extClock, 1000), page->toString()).c_str());

						auto const startTimeInMs = clockToTimescale(page->showTimestamp, 1000);
						auto const durationInMs = clockToTimescale((page->hideTimestamp - page->showTimestamp), 1000);
						page->startTimeInMs = startTimeInMs;
						page->endTimeInMs = startTimeInMs + durationInMs;
						currentPages.push_back(*page);
					}
				}

				i += dataUnitSize;
			}
		}
};

IModule* createObject(KHost* host, void* va) {
	auto config = (TeletextToTtmlConfig*)va;
	enforce(host, "TeletextToTTML: host can't be NULL");
	enforce(config, "TeletextToTTML: config can't be NULL");
	return createModule<TeletextToTTML>(host, config).release();
}

auto const registered = Factory::registerModule("TeletextToTTML", &createObject);
}

