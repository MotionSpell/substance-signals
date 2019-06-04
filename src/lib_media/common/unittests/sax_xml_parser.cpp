#include "tests/tests.hpp"

#include "lib_media/common/sax_xml_parser.hpp"

static const char xmlTestData[] = R"(
<?xml version="1.0" encoding="utf-8"?>
<!-- This is a comment -->
<MPD availabilityStartTime="1970-01-01T00:00:00Z" id="Config part of url maybe?" maxSegmentDuration="PT2S" minBufferTime="PT2S" minimumUpdatePeriod="P100Y" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash-if-simple" publishTime="2019-01-15T15:09:07Z" timeShiftBufferDepth="PT5M" type="dynamic" ns1:schemaLocation="urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd" xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:ns1="http://www.w3.org/2001/XMLSchema-instance">
   <ProgramInformation>
      <Title>Media Presentation Description from DASHI-IF live simulator</Title>
   </ProgramInformation>
   <BaseURL>http://livesim.dashif.org/livesim/testpic_2s/</BaseURL>
<Period id="p0" start="PT0S">
      <AdaptationSet contentType="audio" lang="eng" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
         <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main" />
         <SegmentTemplate duration="2" initialization="$RepresentationID$/init.mp4" media="$RepresentationID$/$Number$.m4s" startNumber="0" />
         <Representation audioSamplingRate="48000" bandwidth="48000" codecs="mp4a.40.2" id="A48">
            <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2" />
         </Representation>
      </AdaptationSet>
      <AdaptationSet contentType="video" maxFrameRate="60/2" maxHeight="360" maxWidth="640" mimeType="video/mp4" minHeight="360" minWidth="640" par="16:9" segmentAlignment="true" startWithSAP="1">
         <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main" />
         <SegmentTemplate duration="2" initialization="$RepresentationID$/init.mp4" media="$RepresentationID$/$Number$.m4s" startNumber="0" />
         <Representation bandwidth="300000" codecs="avc1.64001e" frameRate="60/2" height="360" id="V300" sar="1:1" width="640" />
      </AdaptationSet>
   </Period>
</MPD>
)";

unittest("SAX XML parser: normal") {
  std::vector<std::string> tags;
	auto onNode = [&](std::string name, std::map<std::string, std::string>& attributes) {
		(void)attributes;
    tags.push_back(name);
	};

  auto expected = std::vector<std::string>({
      "MPD",
      "ProgramInformation",
      "Title",
      "BaseURL",
      "Period",
      "AdaptationSet",
      "Role",
      "SegmentTemplate",
      "Representation",
      "AudioChannelConfiguration",
      "AdaptationSet",
      "Role",
      "SegmentTemplate",
      "Representation",
      });

	saxParse(xmlTestData, onNode);

  ASSERT_EQUALS(expected, tags);
}

static const char invalidXmlTestData[] = R"(
<?xml version="1.0" encoding="utf-8"?>
<Hello>
  <World #>
  </World>
</Hello>
)";

unittest("SAX XML parser: invalid") {
  std::vector<std::string> tags;
	auto onNode = [&](std::string, std::map<std::string, std::string>&) { };

	ASSERT_THROWN(saxParse(invalidXmlTestData, onNode));
}

