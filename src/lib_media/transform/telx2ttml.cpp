#include "telx2ttml.hpp"
#include "../common/libav.hpp"
#include "lib_modules/utils/factory.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_utils/log_sink.hpp" // Warning
#include "lib_utils/time.hpp" // timeInMsToStr
#include <vector>
#include <sstream>

extern "C" {
#include <libavcodec/avcodec.h> // AVCodecContext
}

auto const DEBUG_DISPLAY_TIMESTAMPS = false;

namespace Modules {
namespace Transform {

struct Page {
	Page() {
		lines.push_back(make_unique<std::stringstream>());
		ss = lines[0].get();
	}

	std::string toString() const {
		std::stringstream str;
		for (auto &ss : lines) {
			str << ss->str() << std::endl;
		}
		return str.str();
	}

	std::string toTTML(uint64_t startTimeInMs, uint64_t endTimeInMs, uint64_t idx) const {
		std::stringstream ttml;

		if (!lines.empty() || DEBUG_DISPLAY_TIMESTAMPS) {
			const size_t timecodeSize = 24;
			char timecodeShow[timecodeSize] = { 0 };
			timeInMsToStr(startTimeInMs, timecodeShow, ".");
			timecodeShow[timecodeSize-1] = 0;
			char timecodeHide[timecodeSize] = { 0 };
			timeInMsToStr(endTimeInMs, timecodeHide, ".");
			timecodeHide[timecodeSize-1] = 0;

			ttml << "      <p region=\"Region\" style=\"textAlignment_0\" begin=\"" << timecodeShow << "\" end=\"" << timecodeHide << "\" xml:id=\"s" << idx << "\">\n";
			if(DEBUG_DISPLAY_TIMESTAMPS) {
				ttml << "        <span style=\"Style0_0\">" << timecodeShow << " - " << timecodeHide << "</span>\n";
			} else {
				ttml << "        <span style=\"Style0_0\">";

				auto const numLines = lines.size();
				if (numLines > 0) {
					auto const numEffectiveLines = lines[numLines-1]->str().empty() ? numLines-1 : numLines;
					if (numEffectiveLines > 0) {
						for (size_t i = 0; i < numEffectiveLines - 1; ++i) {
							ttml << lines[i]->str() << "<br/>\r\n";
						}
						ttml << lines[numEffectiveLines - 1]->str();
					}
				}

				ttml << "</span>\n";
			}
			ttml << "      </p>\n";
		}
		return ttml.str();
	}

	uint64_t tsInMs=0, startTimeInMs=0, endTimeInMs=0, showTimestamp=0, hideTimestamp=0;
	uint32_t framesProduced = 0;
	std::vector<std::unique_ptr<std::stringstream>> lines;
	std::stringstream *ss = nullptr;
};

struct ITelxConfig {
	virtual ~ITelxConfig() {}
};

}
}

#include "telx.hpp" // requires 'Page' definition

using namespace Modules;
using namespace Transform;

namespace {

class TeletextToTTML : public ModuleS {
	public:
		enum TimingPolicy {
			AbsoluteUTC,     //USP
			RelativeToMedia, //14496-30
			RelativeToSplit  //MSS
		};

		TeletextToTTML(IModuleHost* host, TeletextToTtmlConfig* cfg);
		void process(Data data) override;

	private:
		std::string toTTML(uint64_t startTimeInMs, uint64_t endTimeInMs);
		void sendSample(const std::string &sample);
		void processTelx(Data pkt);
		void dispatch();

		IModuleHost* const m_host;
		std::function<int64_t()> getUtcPipelineStartTime;
		OutputDataDefault<DataAVPacket> *output;
		const unsigned pageNum;
		const std::string lang;
		const TeletextToTtmlConfig::TimingPolicy timingPolicy;
		int64_t intClock = 0, extClock = 0;
		const uint64_t maxPageDurIn180k, splitDurationIn180k;
		std::vector<std::unique_ptr<Page>> currentPages;
		std::unique_ptr<ITelxConfig> config;
};

std::string TeletextToTTML::toTTML(uint64_t startTimeInMs, uint64_t endTimeInMs) {
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
	switch (timingPolicy) {
	case TeletextToTtmlConfig::AbsoluteUTC:
		offsetInMs = getUtcPipelineStartTime();
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
		auto page = currentPages.begin();
		while (page != currentPages.end()) {
			if ((*page)->endTimeInMs > startTimeInMs && (*page)->startTimeInMs < endTimeInMs) {
				auto localStartTimeInMs = std::max<uint64_t>((*page)->startTimeInMs, startTimeInMs);
				auto localEndTimeInMs = std::min<uint64_t>((*page)->endTimeInMs, endTimeInMs);
				m_host->log(Debug, format("[%s-%s]: %s - %s: %s", startTimeInMs, endTimeInMs, localStartTimeInMs, localEndTimeInMs, (*page)->toString()).c_str());
				ttml << (*page)->toTTML(localStartTimeInMs + offsetInMs, localEndTimeInMs + offsetInMs, startTimeInMs / clockToTimescale(this->splitDurationIn180k, 1000));
			}
			if ((*page)->endTimeInMs <= endTimeInMs) {
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

TeletextToTTML::TeletextToTTML(IModuleHost* host, TeletextToTtmlConfig* cfg)
	: m_host(host),
	  getUtcPipelineStartTime(cfg->getUtcPipelineStartTime),
	  pageNum(cfg->pageNum), lang(cfg->lang), timingPolicy(cfg->timingPolicy), maxPageDurIn180k(timescaleToClock(cfg->maxDelayBeforeEmptyInMs, 1000)), splitDurationIn180k(timescaleToClock(cfg->splitDurationInMs, 1000)) {
	enforce(getUtcPipelineStartTime != nullptr, "TeletextToTTML: getUtcPipelineStartTime can't be NULL");
	config = make_unique<Config>();
	addInput(this);
	output = addOutput<OutputDataDefault<DataAVPacket>>();
}

void TeletextToTTML::sendSample(const std::string &sample) {
	auto out = output->getBuffer(0);
	out->setMediaTime(intClock);
	auto pkt = out->getPacket();
	pkt->size = (int)sample.size();
	pkt->data = (uint8_t*)av_malloc(pkt->size);
	pkt->flags |= AV_PKT_FLAG_KEY;
	memcpy(pkt->data, (uint8_t*)sample.c_str(), pkt->size);
	output->emit(out);
}

void TeletextToTTML::dispatch() {
	int64_t prevSplit = (intClock / splitDurationIn180k) * splitDurationIn180k;
	int64_t nextSplit = prevSplit + splitDurationIn180k;
	while ((int64_t)(extClock - maxPageDurIn180k) > nextSplit) {
		sendSample(toTTML(clockToTimescale(prevSplit, 1000), clockToTimescale(nextSplit, 1000)));
		intClock = nextSplit;
		prevSplit = (intClock / splitDurationIn180k) * splitDurationIn180k;
		nextSplit = prevSplit + splitDurationIn180k;
	}
}

void TeletextToTTML::processTelx(Data sub) {
	auto data = sub->data();
	auto &cfg = *dynamic_cast<Config*>(config.get());
	cfg.page = pageNum;
	int i = 1;
	while (i <= int(data.len) - 6) {
		auto dataUnitId = (DataUnit)data.ptr[i++];
		auto const dataUnitSize = data.ptr[i++];
		const uint8_t telxPayloadSize = 44;
		if ( ((dataUnitId == NonSubtitle) || (dataUnitId == Subtitle)) && (dataUnitSize == telxPayloadSize) ) {
			uint8_t entitiesData[telxPayloadSize];
			for (uint8_t j = 0; j < dataUnitSize; j++) {
				auto byte = data.ptr[i + j];
				entitiesData[j] = Reverse8[byte]; //reverse endianess
			}

			auto page = process_telx_packet(cfg, dataUnitId, (Payload*)entitiesData, sub->getMediaTime());
			if (page) {
				m_host->log((int64_t)page->showTimestamp < intClock ? Warning : Debug,
				    format("framesProduced %s, show=%s:hide=%s, clocks:data=%s:int=%s,ext=%s, content=%s",
				        page->framesProduced, clockToTimescale(page->showTimestamp, 1000), clockToTimescale(page->hideTimestamp, 1000),
				        clockToTimescale(sub->getMediaTime(), 1000), clockToTimescale(intClock, 1000), clockToTimescale(extClock, 1000), page->toString()).c_str());

				auto const startTimeInMs = clockToTimescale(page->showTimestamp, 1000);
				auto const durationInMs = clockToTimescale((page->hideTimestamp - page->showTimestamp), 1000);
				page->startTimeInMs = startTimeInMs;
				page->endTimeInMs = startTimeInMs + durationInMs;
				currentPages.push_back(std::move(page));
			}
		}

		i += dataUnitSize;
	}
}

void TeletextToTTML::process(Data data) {
	if (inputs[0]->updateMetadata(data))
		output->setMetadata(data->getMetadata());
	extClock = data->getMediaTime();
	//TODO
	//14. add flush() for ondemand samples
	//15. UTF8 to TTML formatting? accent
	if (data->data().len) {
		processTelx(data);
	}
	dispatch();
}

Modules::IModule* createObject(IModuleHost* host, va_list va) {
	auto config = va_arg(va, TeletextToTtmlConfig*);
	enforce(host, "TeletextToTTML: host can't be NULL");
	enforce(config, "TeletextToTTML: config can't be NULL");
	return Modules::create<TeletextToTTML>(host, config).release();
}

auto const registered = Factory::registerModule("TeletextToTTML", &createObject);
}
