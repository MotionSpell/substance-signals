#include "telx2ttml.hpp"

namespace Modules {
namespace Transform {

TeletextToTTML::TeletextToTTML() {
	/*auto input = */addInput(new Input<DataAVPacket>(this));
	//input->setMetadata(new MetadataRawAudio);
	output = addOutput<OutputDataDefault<DataAVPacket>>();
}

void TeletextToTTML::flush() {
	//TODO: or a new sparse module type? known at pipeline level?
}

void TeletextToTTML::process(Data data) {
	//1. nothing => DONE
	//2. build graph => DONE
	//3. same ttml: fwd input data => DONE
	//4. gpac mux and update deps => DONE
	//5. sparse stream : send regularly empty samples
	//6. real converter

	auto sub = safe_cast<const DataAVPacket>(data);
	if (!sub) //on sparse stream, we may be regularly awaken by other streams? => or event PCR handled at the Pipeline level?
		return;
	auto a = sub->getPacket();
	auto b = sub->getMetadata();
	//std::dynamic_pointer_cast<const MetadataPktLibavSubtitle>(data);//safe_cast<const MetadataPktLibavSubtitle>(data);

	//TODO: filter by page? or a list of page and use one one module to dispatch?

	//AVSubtitle -> AVFrame, you have to use the new M:N
	//API, and it's integrated into lavfi.
	//
	//Now a bit longer : the AVFrame->extended_data are
	//AVFrameSubtitleRectangle(declared in lavu / frame)

	//auto out = output->getBuffer(0);
	//out->setMetadata(data->getMetadata());
	//output->emit(out);
	output->emit(data);
}


/*
<?xml version="1.0" encoding="UTF-8"?><tt xmlns="http://www.w3.org/ns/ttml" xmlns:tts="http://www.w3.org/ns/ttml#styling" xmlns:smpte="http://www.smpte-ra.org/schemas/2052-1/2010/smpte-tt"><head>
<smpte:information smpte:mode="Enhanced" />
<styling>
<style xml:id="emb" tts:fontSize="4.1%" tts:fontFamily="monospaceSansSerif" />
<style xml:id="ttx" tts:fontSize="3.21%" tts:fontFamily="monospaceSansSerif"/>
<style xml:id="backgroundStyle" tts:fontFamily="proportionalSansSerif" tts:fontSize="18px" tts:textAlign="center" tts:origin="0% 66%" tts:extent="100% 33%" tts:backgroundColor="rgba(0,0,0,0)" tts:displayAlign="center" />
<style xml:id="speakerStyle" style="backgroundStyle" tts:color="white" tts:textOutline="black 1px" tts:backgroundColor="transparent" />
<style xml:id="textStyle" style="speakerStyle" tts:color="white" tts:textOutline="none" tts:backgroundColor="black" />
</styling><layout>
<region xml:id="full" tts:origin="0% 0%" tts:extent="100% 100%" tts:zIndex="1" />
<region xml:id="speaker" style="speakerStyle" tts:zIndex="1" />
<region xml:id="background" style="backgroundStyle" tts:zIndex="0" />
</layout>
</head><body><div>
<!-- text-based TTML -->
</div></body></tt>
*/

}
}
