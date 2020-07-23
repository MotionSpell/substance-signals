#include "telx2ttml.hpp"
#include "lib_media/common/attributes.hpp"
#include "lib_modules/utils/factory.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_utils/log_sink.hpp" // Debug
#include "lib_utils/time.hpp" // timeInMsToStr
#include "lib_utils/tools.hpp" // enforce
#include "lib_utils/format.hpp"
#include <algorithm> // std::max
#include <vector>
#include <cassert>

#include "page.hpp"
#include "telx.hpp"
#include <sstream>

using namespace Modules;

extern const int ROWS;
extern const int COLS;

namespace {

auto const DEBUG_DISPLAY_TIMESTAMPS = false;
const std::string defaultColor = "0xffffffff";

std::string timecodeToString(int64_t timeInMs) {
	const size_t timecodeSize = 24;
	char timecode[timecodeSize] = { 0 };
	timeInMsToStr(timeInMs, timecode, ".");
	timecode[timecodeSize - 1] = 0;
	return timecode;
}

std::string pageToString(Page const& page) {
	std::string r;

	for(auto& ss : page.lines)
		r += ss.text + "\n";

	return r;
}

class TeletextToTTML : public ModuleS {
	public:
		TeletextToTTML(KHost* host, TeletextToTtmlConfig* cfg)
			: m_host(host),
			  m_utcStartTime(cfg->utcStartTime),
			  lang(cfg->lang), timingPolicy(cfg->timingPolicy), maxPageDurIn180k(timescaleToClock(cfg->maxDelayBeforeEmptyInMs, 1000)), splitDurationIn180k(timescaleToClock(cfg->splitDurationInMs, 1000)) {
			m_telxState.reset(createTeletextParser(host, cfg->pageNum));
			enforce(cfg->utcStartTime != nullptr, "TeletextToTTML: utcStartTime can't be NULL");
			output = addOutput();
		}

		void processOne(Data data) override {
			if(inputs[0]->updateMetadata(data))
				output->setMetadata(data->getMetadata());

			// TODO
			// 14. add flush() for ondemand samples
			// 15. UTF8 to TTML formatting? accent
			processTelx(data);
			dispatch(data->get<PresentationTime>().time);
		}

	private:
		KHost* const m_host;
		IUtcStartTimeQuery const * const m_utcStartTime;
		OutputDefault* output;
		const std::string lang;
		const TeletextToTtmlConfig::TimingPolicy timingPolicy;
		int64_t intClock = 0;
		const int64_t maxPageDurIn180k, splitDurationIn180k;
		std::vector<Page> currentPages;
		std::unique_ptr<ITeletextParser> m_telxState;

		std::string serializePageToTtml(Page const& page, int64_t startTimeInMs, int64_t endTimeInMs) const {
			auto const timecodeShow = timecodeToString(startTimeInMs);
			auto const timecodeHide = timecodeToString(endTimeInMs);

			assert(!page.lines.empty());

			std::stringstream ttml;

			for (auto& line : page.lines) {
				if (line.text.empty())
					continue;

				ttml << "      <p region=\"Region-" << &page << "-" << line.row << "\" style=\"Style0_0\" begin=\"" << timecodeShow << "\" end=\"" << timecodeHide << "\">\n";
				ttml << "        <span tts:color=\"" << line.color << "\" tts:backgroundColor=\"#000000\">" << line.text << "</span>\n";
				ttml << "      </p>\n";
			}

			return ttml.str();
		}

		std::string toTTML(int64_t startTimeInMs, int64_t endTimeInMs) const {
			int64_t offsetInMs;
			switch (timingPolicy) {
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

			std::stringstream ttml;
			ttml << "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
			ttml << "<tt xmlns=\"http://www.w3.org/ns/ttml\" xmlns:tt=\"http://www.w3.org/ns/ttml\" xmlns:ttm=\"http://www.w3.org/ns/ttml#metadata\" xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" xmlns:ttp=\"http://www.w3.org/ns/ttml#parameter\" xml:lang=\"" << lang << "\" >\n";
			ttml << "  <head>\n";
			ttml << "    <styling>\n";
			ttml << "      <style xml:id=\"Style0_0\" tts:fontSize=\"80%\" tts:fontFamily=\"monospaceSansSerif\" />\n";
			ttml << "    </styling>\n";
			ttml << "    <layout>\n";

			auto display = [=](const Page& page) {
				return page.endTimeInMs > startTimeInMs && page.startTimeInMs < endTimeInMs;
			};

			// We currently assign one Region per line per page with positioning aligned on Teletext input
			// Single or double height
			bool doubleHeight = false;
			for (auto& page : currentPages) {
				if (!display(page))
					continue;

				for (auto& line : page.lines) {
					if (line.text.empty())
						continue;

					if (line.doubleHeight) {
						doubleHeight = true;
					} else if (doubleHeight) {
						m_host->log(Warning, "Mixing single and double height is not handled. Contact your vendor.");
					}

					auto const numLines = 24;
					auto const height = 100.0 / numLines;
					auto const spacingFactor = 1.0;
					auto const verticalOrigin = (100 - spacingFactor * height * ROWS) + spacingFactor * height * line.row; // Percentage. Promote the last rows for text display.
					if (verticalOrigin >= 0 && verticalOrigin + spacingFactor * height * (1 + (int)doubleHeight) <= 100) {
						auto const factorH = 1 + (int)doubleHeight;
						auto const margin = 10.0; //percentage
						auto const origin = (margin + (100 - 2 * margin) * line.col / (double)COLS) / factorH;
						auto const width = 100 - origin - margin;
						ttml << "      <region xml:id=\"Region-" << &page << "-" << line.row << "\" tts:origin=\"" << origin << "% " << verticalOrigin << "%\" tts:extent=\"" << width << "% " << height * factorH << "%\" ";
						ttml << "tts:displayAlign=\"center\" tts:textAlign=\"center\" />\n";
					}
				}
			}

			ttml << "    </layout>\n";
			ttml << "  </head>\n";
			ttml << "  <body>\n";
			ttml << "    <div>\n";

			for(auto& page : currentPages) {
				if (display(page)) {
					auto localStartTimeInMs = std::max<int64_t>(page.startTimeInMs, startTimeInMs);
					auto localEndTimeInMs = std::min<int64_t>(page.endTimeInMs, endTimeInMs);
					m_host->log(Debug, format("[%s-%s]: %s - %s: %s", startTimeInMs, endTimeInMs, localStartTimeInMs, localEndTimeInMs, pageToString(page)).c_str());
					ttml << serializePageToTtml(page, localStartTimeInMs + offsetInMs, localEndTimeInMs + offsetInMs);
				}
			}

			ttml << "    </div>\n";
			ttml << "  </body>\n";
			ttml << "</tt>\n\n";
			return ttml.str();
		}

		void removeOutdatedPages(int64_t endTimeInMs) {
			auto page = currentPages.begin();
			while(page != currentPages.end()) {
				if(page->endTimeInMs <= endTimeInMs) {
					page = currentPages.erase(page);
				} else {
					++page;
				}
			}
		}

		void sendSample(const std::string& sample) {
			auto out = output->allocData<DataRaw>(0);
			out->set(DecodingTime{ intClock });
			out->setMediaTime(intClock);
			out->buffer->resize(sample.size());

			CueFlags flags {};
			flags.keyframe = true;
			out->set(flags);

			memcpy(out->buffer->data().ptr, (uint8_t*)sample.c_str(), sample.size());
			output->post(out);
		}

		void dispatch(int64_t extClock) {
			int64_t prevSplit = (intClock / splitDurationIn180k) * splitDurationIn180k;
			int64_t nextSplit = prevSplit + splitDurationIn180k;

			while(extClock - maxPageDurIn180k > nextSplit) {
				auto const start = clockToTimescale(prevSplit, 1000);
				auto const end = clockToTimescale(nextSplit, 1000);

				if (DEBUG_DISPLAY_TIMESTAMPS) {
					Page pageOut;
					pageOut.startTimeInMs = start;
					pageOut.endTimeInMs = end;
					pageOut.lines.push_back({ timecodeToString(start) + " - " + timecodeToString(end), defaultColor, false, ROWS - 1, 0 });
					currentPages.clear();
					currentPages.push_back(pageOut);
				}

				sendSample(toTTML(start, end));
				removeOutdatedPages(end);

				intClock = nextSplit;
				prevSplit = (intClock / splitDurationIn180k) * splitDurationIn180k;
				nextSplit = prevSplit + splitDurationIn180k;
			}
		}

		void processTelx(Data sub) {
			for(auto& page : m_telxState->parse(sub->data(), sub->get<PresentationTime>().time)) {
				m_host->log(Debug,
				    format("show=%s:hide=%s, clocks:data=%s:int=%s, content=%s",
				        clockToTimescale(page.showTimestamp, 1000), clockToTimescale(page.hideTimestamp, 1000),
				        clockToTimescale(sub->get<PresentationTime>().time, 1000), clockToTimescale(intClock, 1000), pageToString(page)).c_str());

				auto const startTimeInMs = clockToTimescale(page.showTimestamp, 1000);
				auto const durationInMs = clockToTimescale((page.hideTimestamp - page.showTimestamp), 1000);
				page.startTimeInMs = startTimeInMs;
				page.endTimeInMs = startTimeInMs + durationInMs;
				currentPages.push_back(page);
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

