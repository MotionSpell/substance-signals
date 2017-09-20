#include "telx2ttml.hpp"
#include "telx.hpp"
#include "lib_utils/time.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
}

//#define DEBUG_DISPLAY_TIMESTAMPS
#define USP_HACK

#ifdef USP_HACK
#define USP_HACK_OFFSET_IN_MS 0
#define USP_ROUNDUP(t) (((t % 1000) ? ((t / 1000 + 1) * 1000) : t) + USP_HACK_OFFSET_IN_MS)
#else
#define USP_ROUNDUP(t) (t)
#endif

namespace Modules {
namespace Transform {

const std::string Page::toTTML(uint64_t startTimeInMs, uint64_t endTimeInMs, uint64_t idx) const {
	std::stringstream ttml;
#ifndef DEBUG_DISPLAY_TIMESTAMPS
	if (!ss.str().empty())
#endif
	{
		const size_t timecodeSize = 24;
		char timecode_show[timecodeSize] = { 0 };
		timeInMsToStr(startTimeInMs, timecode_show, ".");
		timecode_show[timecodeSize-1] = 0;
		char timecode_hide[timecodeSize] = { 0 };
		timeInMsToStr(endTimeInMs, timecode_hide, ".");
		timecode_hide[timecodeSize-1] = 0;

		ttml << "      <p region=\"Region\" style=\"textAlignment_0\" begin=\"" << timecode_show << "\" end=\"" << timecode_hide << "\" xml:id=\"s" << idx << "\">\n";
#ifdef DEBUG_DISPLAY_TIMESTAMPS
		ttml << "        <span style=\"Style0_0\">" << timecode_show << " - " << timecode_hide << "</span>\n";
#else
		ttml << "        <span style=\"Style0_0\">" << ss.str() << "</span>\n";
#endif
		ttml << "      </p>\n";
	}
	return ttml.str();
}

const std::string Page::toSRT() {
		std::stringstream srt;
		{
			char buf[255];
			snprintf(buf, 255, "%.3f|", (double)tsInMs / 1000.0);
			srt << buf;
		}

		{
			char timecode_show[24] = { 0 };
			timeInMsToStr(showTimestampInMs, timecode_show);
			timecode_show[12] = 0;

			char timecode_hide[24] = { 0 };
			timeInMsToStr(hideTimestampInMs, timecode_hide);
			timecode_hide[12] = 0;

			char buf[255];
			snprintf(buf, 255, "%u\r\n%s --> %s\r\n", (unsigned)++framesProduced, timecode_show, timecode_hide);
			srt << buf;
		}

		srt << ss.str();

		return srt.str();
	}

const std::string TeletextToTTML::toTTML(uint64_t startTimeInMs, uint64_t endTimeInMs) {
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

	int64_t offset;
	switch (timingPolicy) {
	case AbsoluteUTC: offset = USP_ROUNDUP((int64_t)firstDataAbsTimeInMs); break;
	case RelativeToMedia: offset = 0; break;
	case RelativeToSplit: offset = -1 * startTimeInMs; break;
	default: throw error("Unknown timing policy (1)");
	}

#ifdef DEBUG_DISPLAY_TIMESTAMPS
	auto pageOut = uptr(new Page);
	ttml << pageOut->toTTML(offset + startTimeInMs, offset + endTimeInMs, startTimeInMs / clockToTimescale(this->splitDurationIn180k, 1000));
#else
	auto page = currentPages.begin();
	while (page != currentPages.end()) {
		if ((*page)->endTimeInMs > startTimeInMs && (*page)->startTimeInMs < endTimeInMs) {
			auto localStartTimeInMs = std::max<uint64_t>((*page)->startTimeInMs, startTimeInMs);
			auto localEndTimeInMs = std::min<uint64_t>((*page)->endTimeInMs, endTimeInMs);
			log(Debug, "[%s-%s]: %s - %s: %s", startTimeInMs, endTimeInMs, localStartTimeInMs, localEndTimeInMs, (*page)->ss.str());
			ttml << (*page)->toTTML(localStartTimeInMs + offset, localEndTimeInMs + offset, startTimeInMs / clockToTimescale(this->splitDurationIn180k, 1000));
		}
		if ((*page)->endTimeInMs <= endTimeInMs) {
			page = currentPages.erase(page);
		} else {
			++page;
		}
	}
#endif /*DEBUG_DISPLAY_TIMESTAMPS*/

	ttml << "    </div>\n";
	ttml << "  </body>\n";
	ttml << "</tt>\n\n";
	return ttml.str();
}

TeletextToTTML::TeletextToTTML(unsigned pageNum, const std::string &lang, uint64_t splitDurationInMs, TimingPolicy timingPolicy)
: pageNum(pageNum), lang(lang), timingPolicy(timingPolicy), splitDurationIn180k(timescaleToClock(splitDurationInMs, 1000)) {
	addInput(new Input<DataAVPacket>(this));
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
	while ((int64_t)(extClock - delayIn180k) > nextSplit) {
		sendSample(toTTML(clockToTimescale(prevSplit, 1000), clockToTimescale(nextSplit, 1000)));
		intClock = nextSplit;
		prevSplit = (intClock / splitDurationIn180k) * splitDurationIn180k;
		nextSplit = prevSplit + splitDurationIn180k;
	}
}

void TeletextToTTML::processTelx(DataAVPacket const * const sub) {
	auto pkt = sub->getPacket();
	config.page = pageNum;
	int i = 1;
	while (i <= pkt->size - 6) {
		auto dataUnitId = (DataUnit)pkt->data[i++];
		auto const dataUnitSize = pkt->data[i++];
		const uint8_t telxPayloadSize = 44;
		if ( ((dataUnitId == NONSUBTITLE) || (dataUnitId == SUBTITLE)) && (dataUnitSize == telxPayloadSize) ) {
			uint8_t entitiesData[telxPayloadSize];
			for (uint8_t j = 0; j < dataUnitSize; j++) {
				entitiesData[j] = Reverse8[pkt->data[i + j]]; //reverse endianess
			}

			auto page = process_telx_packet(dataUnitId, (Payload*)entitiesData, pkt->pts);
			if (page) {
				auto const codecCtx = safe_cast<const MetadataPktLibav>(sub->getMetadata())->getAVCodecContext();
				log(Debug, "framesProduced %s, show=%s, hide=%s", page->framesProduced, convertToTimescale(page->showTimestampInMs * codecCtx->pkt_timebase.num, codecCtx->pkt_timebase.den, 1000), convertToTimescale(page->hideTimestampInMs * codecCtx->pkt_timebase.num, codecCtx->pkt_timebase.den, 1000));
				if (sub->getMediaTime() < intClock) {
					log(Warning, "Timing error: received %s but internal clock is already at %s", sub->getMediaTime(), intClock);
				}

				auto const startTimeInMs = std::max<int64_t>(convertToTimescale(pkt->pts * codecCtx->pkt_timebase.num, codecCtx->pkt_timebase.den, 1000), convertToTimescale(page->showTimestampInMs * codecCtx->pkt_timebase.num, codecCtx->pkt_timebase.den, 1000));
				auto const durationInMs = convertToTimescale((page->hideTimestampInMs - page->showTimestampInMs) * codecCtx->pkt_timebase.num, codecCtx->pkt_timebase.den, 1000);
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
	if (!firstDataAbsTimeInMs)
		firstDataAbsTimeInMs = (uint64_t)(1000 * getUTC());
	extClock = data->getMediaTime();
	//TODO
	//14. add flush() for ondemand samples
	//15. UTF8 to TTML formatting? accent + EOLs </br>
	auto sub = safe_cast<const DataAVPacket>(data);
	if (sub->size()) {
		processTelx(sub.get());
	}
	dispatch();
}

}
}
