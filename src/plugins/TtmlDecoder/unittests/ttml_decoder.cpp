#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_modules/utils/loader.hpp"
#include "lib_media/common/subtitle.hpp"
#include "plugins/TtmlEncoder/ttml_encoder.hpp"
#include "../ttml_decoder.hpp"

using namespace Modules;

// allows ASSERT_EQUALS on Page
static std::ostream& operator<<(std::ostream& o, const Page& p) {
	o << p.toString();
	return o;
}

static bool operator!=(const Page::Line& lhs, const Page::Line& rhs) {
	return lhs.text != rhs.text
	    || lhs.color != rhs.color
	    || lhs.doubleHeight != rhs.doubleHeight
	    || lhs.row != rhs.row
	    || lhs.col != rhs.col;
}

static bool operator!=(const Page& lhs, const Page& rhs) {
	if (lhs.lines.size() != rhs.lines.size())
		return true;

	for (size_t i=0; i<lhs.lines.size(); ++i)
		if (lhs.lines[i] != rhs.lines[i])
			return true;

	return lhs.showTimestamp != rhs.showTimestamp
	    || lhs.hideTimestamp != rhs.hideTimestamp;
}

unittest("ttml_decoder: ttml_encoder sample") {
	TtmlEncoderConfig encCfg;
	encCfg.splitDurationInMs = 1000;
	encCfg.maxDelayBeforeEmptyInMs = 2000;
	encCfg.timingPolicy = TtmlEncoderConfig::RelativeToMedia;
	auto enc = loadModule("TTMLEncoder", &NullHost, &encCfg);

	TtmlDecoderConfig decCfg;
	auto dec = loadModule("TTMLDecoder", &NullHost, &decCfg);

	ConnectOutputToInput(enc->getOutput(0), dec->getInput(0));

	auto const pageNum = 2;
	auto const pageDurationIn180k = timescaleToClock(pageNum * encCfg.splitDurationInMs + encCfg.maxDelayBeforeEmptyInMs, 1000);
	Page pageSent {0, pageDurationIn180k, std::vector<Page::Line>({{"toto"}, {"titi", "#ff0000"}})};
	auto data = std::make_shared<DataSubtitle>(0);
	auto const time = pageSent.showTimestamp + pageDurationIn180k;
	data->setMediaTime(time);
	data->page = pageSent;

	int received = 0;
	ConnectOutput(dec->getOutput(0), [&](Data data) {
		received++;
		auto &pageReceived = safe_cast<const DataSubtitle>(data)->page;
		pageSent.hideTimestamp = 0; // there is no way to deduce this value here
		ASSERT_EQUALS(pageSent, pageReceived);
	});

	enc->getInput(0)->push(data);

	ASSERT_EQUALS(pageNum, received);
}

unittest("ttml_encoder: ebu-tt-live") {
	std::string ttml = R"|(<?xml version="1.0" encoding="UTF-8"?>
<tt:tt xmlns:tt="http://www.w3.org/ns/ttml" xmlns:ttp="http://www.w3.org/ns/ttml#parameter" xmlns:tts="http://www.w3.org/ns/ttml#styling" xmlns:ebuttm="urn:ebu:tt:metadata" xmlns:ebuttp="urn:ebu:tt:parameters" xmlns:ebutts="urn:ebu:tt:style" xml:lang="de" ttp:cellResolution="50 30" ttp:timeBase="clock" ttp:clockMode="local" ebuttp:sequenceIdentifier="TestSequence1" ebuttp:sequenceNumber="1636064848635">
    <tt:head>
        <tt:metadata>
            <ebuttm:documentMetadata>
                <ebuttm:documentEbuttVersion>v1.0</ebuttm:documentEbuttVersion>
            </ebuttm:documentMetadata>
        </tt:metadata>
        <tt:styling>
            <tt:style xml:id="defaultStyle" tts:fontFamily="Verdana,Arial,Tiresias" tts:fontSize="160%" tts:lineHeight="125%"/>
            <tt:style xml:id="textWhite" tts:color="#FFFFFF" tts:backgroundColor="#000000c2"/><tt:style xml:id="textCenter" tts:textAlign="center"/>
        </tt:styling>
        <tt:layout>
            <tt:region xml:id="bottom" tts:origin="10% 10%" tts:extent="80% 80%" tts:displayAlign="after"/>
        </tt:layout>
    </tt:head>
    <tt:body dur="00:00:30.000">
        <tt:div style="defaultStyle">
            <tt:p xml:id="sub1" style="textCenter" region="bottom">
                <tt:span style="textWhite">Sample of a EBU-TT-LIVE document - line 1</tt:span>
                <tt:br/>
                <tt:span style="textWhite">Sample of a EBU-TT-LIVE document - line 2</tt:span>
            </tt:p>
        </tt:div>
    </tt:body>
</tt:tt>)|";

	TtmlDecoderConfig cfg;
	auto dec = loadModule("TTMLDecoder", &NullHost, &cfg);
	int received = 0;
	Page expected = { 0, 0, { { "Sample of a EBU-TT-LIVE document - line 1", "#FFFFFF" }, { "Sample of a EBU-TT-LIVE document - line 2", "#FFFFFF" } } };
	ConnectOutput(dec->getOutput(0), [&](Data data) {
		auto &pageReceived = safe_cast<const DataSubtitle>(data)->page;
		ASSERT_EQUALS(expected, pageReceived);
		received++;
	});

	auto pkt = std::make_shared<DataRaw>(ttml.size());
	memcpy(pkt->buffer->data().ptr, ttml.data(), ttml.size());
	pkt->setMediaTime(1789);
	dec->getInput(0)->push(pkt);

	ASSERT_EQUALS(1, received);
}
