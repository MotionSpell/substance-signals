#include "adaptive_streaming_common.hpp"
#include "lib_modules/core/data_utc.hpp"
#include "lib_utils/time.hpp"
#include "lib_utils/os.hpp"
#include "lib_utils/default_clock.hpp"
#include <cstring> // memcpy
#include <cassert>
#include <thread>
#include <chrono>

#define MOVE_FILE_NUM_RETRY 3

namespace Modules {
namespace Stream {

AdaptiveStreamingCommon::AdaptiveStreamingCommon(Type type, uint64_t segDurationInMs, const std::string &manifestDir, AdaptiveStreamingCommonFlags flags)
	: type(type), segDurationInMs(segDurationInMs), manifestDir(manifestDir), flags(flags) {
	if ((flags & ForceRealDurations) && !segDurationInMs)
		throw error("Inconsistent parameters: ForceRealDurations flag requires a non-null segment duration.");
	if (!manifestDir.empty() && (flags & SegmentsNotOwned))
		throw error(format("Inconsistent parameters: manifestDir (%s) should be empty when segments are not owned.", manifestDir));
	addInput(new Input<DataRaw>(this));
	outputSegments = addOutput<OutputDefault>();
	outputManifest = addOutput<OutputDefault>();
}

bool AdaptiveStreamingCommon::moveFile(const std::string &src, const std::string &dst) const {
	if (src.empty())
		return true;

	if(src == dst)
		return false; // nothing to do

	if (flags & SegmentsNotOwned)
		throw error(format("Segments not owned require similar filenames (%s != %s)", src, dst));

	auto subdir = dst.substr(0, dst.find_last_of("/") + 1);
	if (!dirExists(subdir))
		mkdir(subdir);

	int retry = MOVE_FILE_NUM_RETRY + 1;
	while (--retry) {
		try {
			::moveFile(src, dst);
			break;
		} catch(...) {
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	if (!retry) {
		return false;
	}
	return true;
}

void AdaptiveStreamingCommon::processInitSegment(Quality const * const quality, size_t index) {
	auto const &meta = quality->getMeta();
	switch (meta->getStreamType()) {
	case AUDIO_PKT: case VIDEO_PKT: case SUBTITLE_PKT: {
		auto out = make_shared<DataBaseRef>(quality->lastData);
		std::string initFn = safe_cast<const MetadataFile>(quality->lastData->getMetadata())->filename;
		if (initFn.empty()) {
			initFn = format("%s%s", manifestDir, getInitName(quality, index));
		} else if (!(flags & SegmentsNotOwned)) {
			auto const dst = format("%s%s", manifestDir, getInitName(quality, index));
			moveFile(initFn, dst);
			initFn = dst;
		}
		out->setMetadata(make_shared<const MetadataFile>(initFn, SEGMENT, meta->mimeType, meta->codecName, meta->durationIn180k, meta->filesize, meta->latencyIn180k, meta->startsWithRAP, true));
		out->setMediaTime(totalDurationInMs, 1000);
		outputSegments->emit(out);
		break;
	}
	default: break;
	}
}

std::string AdaptiveStreamingCommon::getInitName(Quality const * const quality, size_t index) const {
	switch (quality->getMeta()->getStreamType()) {
	case AUDIO_PKT: case VIDEO_PKT: case SUBTITLE_PKT: return format("%s-init.mp4", getPrefix(quality, index));
	default: return "";
	}
}

std::string AdaptiveStreamingCommon::getPrefix(Quality const * const quality, size_t index) const {
	switch (quality->getMeta()->getStreamType()) {
	case AUDIO_PKT:    return format("%s%s", quality->prefix, getCommonPrefixAudio(index));
	case VIDEO_PKT:    return format("%s%s", quality->prefix, getCommonPrefixVideo(index, quality->getMeta()->resolution));
	case SUBTITLE_PKT: return format("%s%s", quality->prefix, getCommonPrefixSubtitle(index));
	default: return "";
	}
}

std::string AdaptiveStreamingCommon::getSegmentName(Quality const * const quality, size_t index, const std::string &segmentNumSymbol) const {
	switch (quality->getMeta()->getStreamType()) {
	case AUDIO_PKT: case VIDEO_PKT: case SUBTITLE_PKT: return format("%s-%s.m4s", getPrefix(quality, index), segmentNumSymbol);
	default: return "";
	}
}

uint64_t AdaptiveStreamingCommon::getCurSegNum() const {
	return (startTimeInMs + totalDurationInMs) / segDurationInMs;
}

void AdaptiveStreamingCommon::ensurePrefix(size_t i) {
	if (qualities[i]->prefix.empty()) {
		qualities[i]->prefix = format("%s/", getPrefix(qualities[i].get(), i));
		//if (!(flags & SegmentsNotOwned)) //FIXME: HLS manifests still requires the subdir presence
		{
			auto const dir = format("%s%s", manifestDir, qualities[i]->prefix);
			if (!dirExists(dir))
				mkdir(dir);
		}
	}
}

void AdaptiveStreamingCommon::endOfStream() {
	if (workingThread.joinable()) {
		for (size_t i = 0; i < inputs.size(); ++i) {
			inputs[i]->push(nullptr);
		}
		workingThread.join();
	}
}

std::shared_ptr<DataBase> AdaptiveStreamingCommon::getPresignalledData(uint64_t size, Data &data, bool EOS) {
	if (!(flags & PresignalNextSegment)) {
		return make_shared<DataBaseRef>(data);
	}
	if (!safe_cast<const MetadataFile>(data->getMetadata())->filename.empty() && !EOS) {
		return nullptr;
	}

	static constexpr uint8_t mp4StaticHeader[] = {
		0x00, 0x00, 0x00, 0x18, 0x73, 0x74, 0x79, 0x70,
		0x6d, 0x73, 0x64, 0x68, 0x00, 0x00, 0x00, 0x00,
		0x6d, 0x73, 0x64, 0x68, 0x6d, 0x73, 0x69, 0x78,
	};
	auto constexpr headerSize = sizeof(mp4StaticHeader);
	if (size == 0 && !EOS) {
		auto out = outputSegments->getBuffer(0);
		out->resize(headerSize);
		memcpy(out->data(), mp4StaticHeader, headerSize);
		return out;
	} else {
		auto dataRaw = safe_cast<const DataRaw>(data);
		auto const dataRawSize = dataRaw->size();
		if (dataRawSize >= headerSize && !memcmp(dataRaw->data(), mp4StaticHeader, headerSize)) {
			auto out = outputSegments->getBuffer(0);
			auto const size = (size_t)(dataRawSize - headerSize);
			out->resize(size);
			memcpy(out->data(), dataRaw->data() + headerSize, size);
			return out;
		} else {
			assert(dataRawSize < 8 || *(uint32_t*)(dataRaw->data() + 4) != (uint32_t)0x70797473);
			return make_shared<DataBaseRef>(data);
		}
	}
}

void AdaptiveStreamingCommon::threadProc() {
	log(Info, "start processing at UTC: %sms.", (uint64_t)Modules::absUTCOffsetInMs);

	auto const numInputs = getNumInputs() - 1;
	for (int i = 0; i < numInputs; ++i) {
		qualities.push_back(createQuality());
	}

	Data data;
	std::vector<uint64_t> curSegDurIn180k(numInputs);
	int i;

	auto isComplete = [&]()->bool {
		uint64_t minIncompletSegDur = std::numeric_limits<uint64_t>::max();
		for (size_t idx = 0; idx < curSegDurIn180k.size(); ++idx) {
			auto const &segDur = curSegDurIn180k[idx];
			if ( (segDur < minIncompletSegDur) &&
			    ((segDur < timescaleToClock(segDurationInMs, 1000)) || (!qualities[idx]->getMeta() || !qualities[idx]->getMeta()->EOS))) {
				minIncompletSegDur = segDur;
			}
		}
		return (minIncompletSegDur == std::numeric_limits<uint64_t>::max()) || (curSegDurIn180k[i] > minIncompletSegDur);
	};
	auto ensureStartTime = [&]() {
		if (!startTimeInMs) startTimeInMs = clockToTimescale(data->getMediaTime(), 1000);
	};
	auto ensureCurDur = [&]() {
		for (i = 0; i < numInputs; ++i) {
			if (!curSegDurIn180k[0])
				curSegDurIn180k[0] = segDurationInMs;
		}
	};
	auto sendLocalData = [&](uint64_t size, bool EOS) {
		ensureStartTime();
		auto out = getPresignalledData(size, data, EOS);
		if (out) {
			auto const &meta = qualities[i]->getMeta();
			out->setMetadata(make_shared<const MetadataFile>(getSegmentName(qualities[i].get(), i, std::to_string(getCurSegNum())), SEGMENT, meta->mimeType, meta->codecName, meta->durationIn180k, size, meta->latencyIn180k, meta->startsWithRAP, EOS));
			out->setMediaTime(totalDurationInMs + timescaleToClock(curSegDurIn180k[i], 1000));
			outputSegments->emit(out);
		}
	};
	auto segmentReady = [&]()->bool {
		ensureCurDur();
		for (i = 0; i < numInputs; ++i) {
			if (curSegDurIn180k[i] < timescaleToClock(segDurationInMs, 1000)) {
				return false;
			}
			if (!qualities[i]->getMeta()->EOS) {
				return false;
			}
		}
		for (auto &d : curSegDurIn180k) {
			d -= timescaleToClock(segDurationInMs, 1000);
		}
		return true;
	};
	for (;;) {
		for (i = 0; i < numInputs; ++i) {
			if (isComplete()) {
				continue;
			}

			if ((type == LiveNonBlocking) && (!qualities[i]->getMeta())) {
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
				qualities[i]->lastData = data;
				auto const &meta = qualities[i]->getMeta();
				if (!meta)
					throw error(format("Unknown data received on input %s", i));
				ensurePrefix(i);

				auto const curDurIn180k = meta->durationIn180k;
				if (curDurIn180k == 0 && curSegDurIn180k[i] == 0) {
					processInitSegment(qualities[i].get(), i);
					if (flags & PresignalNextSegment) {
						sendLocalData(0, false);
					}
					--i; data = nullptr; continue;
				}

				if (segDurationInMs && curDurIn180k) {
					auto const numSeg = totalDurationInMs / segDurationInMs;
					qualities[i]->avg_bitrate_in_bps = ((meta->filesize * 8 * IClock::Rate) / meta->durationIn180k + qualities[i]->avg_bitrate_in_bps * numSeg) / (numSeg + 1);
				}
				if (flags & ForceRealDurations) {
					curSegDurIn180k[i] += meta->durationIn180k;
				} else {
					curSegDurIn180k[i] = segDurationInMs ? timescaleToClock(segDurationInMs, 1000) : meta->durationIn180k;
				}
				if (curSegDurIn180k[i] < timescaleToClock(segDurationInMs, 1000) || !meta->EOS) {
					sendLocalData(meta->filesize, meta->EOS);
				}
			}
		}
		if (!data) {
			if (i != numInputs) {
				break;
			} else {
				assert((type == LiveNonBlocking) && ((int)qualities.size() < numInputs));
				g_DefaultClock->sleep(Fraction(1, 1000));
				continue;
			}
		}
		const int64_t curMediaTimeInMs = clockToTimescale(data->getMediaTime(), 1000);
		ensureStartTime();
		data = nullptr;

		if (segmentReady()) {
			generateManifest();
			totalDurationInMs += segDurationInMs;
			log(Info, "Processes segment (total processed: %ss, UTC: %sms (deltaAST=%s, deltaInput=%s).",
			    (double)totalDurationInMs / 1000, getUTC().num, getUTC().num - startTimeInMs, (int64_t)(getUTC().num - curMediaTimeInMs));

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
