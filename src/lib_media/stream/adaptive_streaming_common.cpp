#include "adaptive_streaming_common.hpp"
#include "lib_utils/time.hpp"

namespace Modules {
namespace Stream {

AdaptiveStreamingCommon::AdaptiveStreamingCommon(Type type, uint64_t segDurationInMs, const std::string &manifestDir, AdaptiveStreamingCommonFlags flags)
: type(type), segDurationInMs(segDurationInMs), manifestDir(manifestDir), flags(flags) {
	if ((flags & ForceRealDurations) && !segDurationInMs)
		throw error("Inconsistent parameters: ForceRealDurations flag requires a non-null segment duration.");
	if (!manifestDir.empty() && (flags & SegmentsNotOwned))
		throw error(format("Inconsistent parameters: manifestDir (%s) should be empty when segments are not owned.", manifestDir));
	addInput(new Input<DataRaw>(this));
	outputSegments = addOutput<OutputDataDefault<DataAVPacket>>();
	outputManifest = addOutput<OutputDataDefault<DataAVPacket>>();
}

std::string AdaptiveStreamingCommon::getInitName(Quality const * const quality, size_t index) const {
	switch (quality->meta->getStreamType()) {
	case AUDIO_PKT: case VIDEO_PKT: case SUBTITLE_PKT: return format("%s-init.m4s", getPrefix(quality, index));
	default: return "";
	}
}

std::string AdaptiveStreamingCommon::getPrefix(Quality const * const quality, size_t index) const {
	switch (quality->meta->getStreamType()) {
	case AUDIO_PKT:    return format("%sa_%s", quality->prefix, index);
	case VIDEO_PKT:    return format("%sv_%s_%sx%s", quality->prefix, index, quality->meta->resolution[0], quality->meta->resolution[1]);
	case SUBTITLE_PKT: return format("%ss_%s", quality->prefix, index);
	default: return "";
	}
}

std::string AdaptiveStreamingCommon::getSegmentName(Quality const * const quality, size_t index, const std::string &segmentNumSymbol) const {
	switch (quality->meta->getStreamType()) {
	case AUDIO_PKT: case VIDEO_PKT: case SUBTITLE_PKT: return format("%s-%s.m4s", getPrefix(quality, index), segmentNumSymbol);
	default: return "";
	}
}

uint64_t AdaptiveStreamingCommon::getCurSegNum() const {
	return (startTimeInMs + totalDurationInMs) / segDurationInMs;
}

void AdaptiveStreamingCommon::endOfStream() {
	if (workingThread.joinable()) {
		for (size_t i = 0; i < inputs.size(); ++i) {
			inputs[i]->push(nullptr);
		}
		workingThread.join();
	}
}

void AdaptiveStreamingCommon::threadProc() {
	log(Info, "start processing at UTC: %sms.", (uint64_t)DataBase::absUTCOffsetInMs);

	auto const numInputs = getNumInputs() - 1;
	qualities.resize(numInputs);
	for (size_t i = 0; i < numInputs; ++i) {
		qualities[i] = createQuality();
	}

	Data data;
	uint64_t curSegDurInMs = 0;
	for (;;) {
		size_t i;
		for (i = 0; i < numInputs; ++i) {
			if ((type == LiveNonBlocking) && (!qualities[i]->meta)) {
				if (inputs[i]->tryPop(data) && !data) {
					break;
				}
			} else {
				data = inputs[i]->pop();
				if (!data) {
					break;
				}
			}
			
			if (data) {
				qualities[i]->meta = safe_cast<const MetadataFile>(data->getMetadata());
				if (!qualities[i]->meta)
					throw error(format("Unknown data received on input %s", i));
				if (qualities[i]->meta->getDuration() == 0) {
					processInitSegment(qualities[i].get(), i);
					--i; data = nullptr; continue;
				}
				qualities[i]->avg_bitrate_in_bps = ((qualities[i]->meta->getSize() * 8 * Clock::Rate) / qualities[i]->meta->getDuration() + qualities[i]->avg_bitrate_in_bps * numSeg) / (numSeg + 1);
				if (qualities[i]->prefix.empty()) {
					qualities[i]->prefix = format("%s_%sK/", getPrefix(qualities[i].get(), i), qualities[i]->avg_bitrate_in_bps / (8 * 1024));
					if (!(flags & SegmentsNotOwned)) {
						auto const dir = format("%s%s", manifestDir, qualities[i]->prefix);
						if ((gf_dir_exists(dir.c_str()) == GF_FALSE) && gf_mkdir(qualities[i]->prefix.c_str()))
							throw std::runtime_error(format("couldn't create subdir %s: please check you have sufficient rights", qualities[i]->prefix));
					}
				}
				if (!i) {
					if (flags & ForceRealDurations) {
						curSegDurInMs += clockToTimescale(qualities[i]->meta->getDuration(), 1000);
					} else {
						curSegDurInMs = segDurationInMs ? segDurationInMs : clockToTimescale(qualities[i]->meta->getDuration(), 1000);
					}
				}
			}
		}
		if (!data) {
			if (i != numInputs) {
				break;
			} else {
				assert((type == LiveNonBlocking) && (qualities.size() < numInputs));
				g_DefaultClock->sleep(Fraction(1, 1000));
				continue;
			}
		}

		numSeg++;
		if (!curSegDurInMs) curSegDurInMs = segDurationInMs;
		if (!startTimeInMs) startTimeInMs = clockToTimescale(data->getMediaTime(), 1000);
		generateManifest();
		totalDurationInMs += curSegDurInMs;
		log(Info, "Processes segment (total processed: %ss, UTC: %sms (deltaAST=%s, deltaInput=%s).",
			(double)totalDurationInMs / 1000, getUTC().num, gf_net_get_utc() - startTimeInMs,
			data ? (int64_t)(gf_net_get_utc() - clockToTimescale(data->getMediaTime(), 1000)) : 0);
		data = nullptr;
		curSegDurInMs = 0;

		if (type != Static) {
			const int64_t durInMs = startTimeInMs + totalDurationInMs - getUTC().num;
			if (durInMs > 0) {
				log(Debug, "Going to sleep for %s ms.", durInMs);
				clock->sleep(Fraction(durInMs, 1000));
			} else {
				log(Warning, "Late from %s ms.", -durInMs);
			}
		}
	}

	/*final rewrite of MPD in static mode*/
	finalizeManifest();
}

void AdaptiveStreamingCommon::process() {
	if (!workingThread.joinable() && (startTimeInMs==(uint64_t)-1)) {
		startTimeInMs = 0;
		workingThread = std::thread(&AdaptiveStreamingCommon::threadProc, this);
	}
}

void AdaptiveStreamingCommon::flush() {
	if (type != Static) {
		endOfStream();
	}
}

}
}
