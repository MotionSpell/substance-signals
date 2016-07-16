#include "adaptive_streaming_common.hpp"
#include "../common/libav.hpp"
#include "lib_gpacpp/gpacpp.hpp"

namespace Modules {
namespace Stream {

AdaptiveStreamingCommon::AdaptiveStreamingCommon(Type type, uint64_t segDurationInMs)
	: type(type), startTimeInMs(0), segDurationInMs(segDurationInMs), totalDurationInMs(0) {
	addInput(new Input<DataAVPacket>(this));
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
		qualities[i] = std::move(createQuality());
	}

	Data data;
	for (;;) {
		for (size_t i = 0; i < numInputs; ++i) {
			data = inputs[i]->pop();
			if (!data) {
				break;
			} else {
				qualities[i]->meta = safe_cast<const MetadataFile>(data->getMetadata());
				if (!qualities[i]->meta)
					throw error(format("Unknown data received on input %s", i).c_str());
				auto const numSeg = totalDurationInMs / segDurationInMs;
				qualities[i]->avg_bitrate_in_bps = (qualities[i]->meta->getSize() * 8 + qualities[i]->avg_bitrate_in_bps * numSeg) / (numSeg + 1);
			}
		}
		if (!data) {
			break;
		}

		if (!startTimeInMs) {
			startTimeInMs = gf_net_get_utc() - segDurationInMs;
		}
		generateManifest();
		totalDurationInMs += segDurationInMs;
		log(Info, "Processes segment (total processed: %ss, UTC: %s (deltaAST=%s).", (double)totalDurationInMs / 1000, gf_net_get_utc(), gf_net_get_utc() - startTimeInMs);

		if (type == Live) {
			auto dur = std::chrono::milliseconds(startTimeInMs + totalDurationInMs - gf_net_get_utc());
			log(Info, "Going to sleep for %s ms.", std::chrono::duration_cast<std::chrono::milliseconds>(dur).count());
			std::this_thread::sleep_for(dur);
		}
	}

	/*final rewrite of MPD in static mode*/
	finalizeManifest();
}

void AdaptiveStreamingCommon::process() {
	if (!workingThread.joinable()) {
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