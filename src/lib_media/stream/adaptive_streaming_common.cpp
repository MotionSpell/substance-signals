#include "adaptive_streaming_common.hpp"
#include "lib_modules/core/clock.hpp"

namespace Modules {
namespace Stream {

AdaptiveStreamingCommon::AdaptiveStreamingCommon(Type type, uint64_t segDurationInMs)
	: type(type), segDurationInMs(segDurationInMs) {
	addInput(new Input<DataRaw>(this));
	outputSegments = addOutput<OutputDataDefault<DataAVPacket>>();
	outputManifest = addOutput<OutputDataDefault<DataAVPacket>>();
}

void AdaptiveStreamingCommon::endOfStream() {
	if (workingThread.joinable()) {
		for (size_t i = 0; i < inputs.size(); ++i) {
			inputs[i]->push(nullptr);
		}
		workingThread.join();
	}
}

//needed because of the use of system time for live - otherwise awake on data as for any multi-input module
//TODO: add clock to the scheduler, see #14
void AdaptiveStreamingCommon::threadProc() {
	log(Info, "start processing at UTC: %s.", gf_net_get_utc());

	auto const numInputs = getNumInputs() - 1;
	qualities.resize(numInputs);
	for (size_t i = 0; i < numInputs; ++i) {
		qualities[i] = createQuality();
	}

	Data data;
	uint64_t curSegDurInMs = 0;
	for (;;) {
		for (size_t i = 0; i < numInputs; ++i) {
			data = inputs[i]->pop();
			if (!data) {
				break;
			} else {
				qualities[i]->meta = safe_cast<const MetadataFile>(data->getMetadata());
				if (!qualities[i]->meta)
					throw error(format("Unknown data received on input %s", i));
				qualities[i]->avg_bitrate_in_bps = ((qualities[i]->meta->getSize() * 8) / (segDurationInMs / 1000.0) + qualities[i]->avg_bitrate_in_bps * numSeg) / (numSeg + 1);
				if (!i) curSegDurInMs = segDurationInMs ? segDurationInMs : clockToTimescale(qualities[i]->meta->getDuration(), 1000);
				if (!startTimeInMs) startTimeInMs = clockToTimescale(data->getMediaTime(), 1000);
			}
		}
		if (!data) {
			break;
		}

		numSeg++;
		if (!startTimeInMs) startTimeInMs = gf_net_get_utc() - curSegDurInMs;
		generateManifest();
		totalDurationInMs += curSegDurInMs;
		log(Info, "Processes segment (total processed: %ss, UTC: %s (deltaAST=%s, deltaInput=%s).", (double)totalDurationInMs / 1000, gf_net_get_utc(), gf_net_get_utc() - startTimeInMs, (int64_t)(gf_net_get_utc() - clockToTimescale(data->getMediaTime(), 1000)));

		if (type == Live) {
			const int64_t dur = startTimeInMs + totalDurationInMs - gf_net_get_utc();
			if (dur > 0) {
				auto durInMs = std::chrono::milliseconds(dur);
				log(Debug, "Going to sleep for %s ms.", dur);
				std::this_thread::sleep_for(durInMs);
			} else {
				log(Warning, "Late from %s ms.", -dur);
			}
		}
	}

	/*final rewrite of MPD in static mode*/
	finalizeManifest();
}

void AdaptiveStreamingCommon::process() {
	if (!workingThread.joinable() && (startTimeInMs==(uint64_t)-1)) {
		startTimeInMs = 0;
		numDataQueueNotify = (int)getNumInputs() - 1; //FIXME: connection/disconnection cannot occur dynamically. Lock inputs?
		workingThread = std::thread(&AdaptiveStreamingCommon::threadProc, this);
	}
}

void AdaptiveStreamingCommon::flush() {
	numDataQueueNotify--;
	if ((type == Live) && (numDataQueueNotify == 0)) {
		endOfStream();
	}
}

}
}
