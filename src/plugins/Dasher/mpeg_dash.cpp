#include "mpeg_dash.hpp"
#include "lib_media/common/gpacpp.hpp"
#include "lib_media/common/metadata_file.hpp"
#include "lib_media/common/attributes.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_modules/utils/helper_dyn.hpp"
#include "lib_modules/utils/factory.hpp"
#include "lib_utils/time.hpp"
#include "lib_utils/tools.hpp" // safe_cast, enforce
#include "lib_utils/log_sink.hpp"
#include "lib_utils/format.hpp"
#include <algorithm> //std::max

#define DASH_TIMESCALE 1000 // /!\ there are some ms already hardcoded from the GPAC calls
#define MIN_UPDATE_PERIOD_FACTOR 1 //should be 0, but dash.js doesn't support MPDs with no refresh time
#define MIN_BUFFER_TIME_IN_MS_VOD  3000
#define MIN_BUFFER_TIME_IN_MS_LIVE 2000
#define AVAILABILITY_TIMEOFFSET_IN_S 0.0
#define PERIOD_NAME "1"
#define MOVE_FILE_NUM_RETRY 3

static auto const g_profiles = "urn:mpeg:dash:profile:isoff-live:2011, http://dashif.org/guidelines/dash264";

using namespace Modules;

namespace {

struct Quality {
	std::shared_ptr<const MetadataFile> getMeta() const {
		return lastData ? safe_cast<const MetadataFile>(lastData->getMetadata()) : nullptr;
	};

	Data lastData;
	uint64_t avg_bitrate_in_bps = 0;
	std::string prefix; //typically a subdir, ending with a folder separator '/'

	GF_MPD_Representation *rep = nullptr;
	struct PendingSegment {
		uint64_t durationIn180k;
		std::string filename;
	};
	std::vector<PendingSegment> timeshiftSegments;
};

struct AdaptiveStreamer : ModuleDynI {
		/*called each time segments are ready*/
		virtual void generateManifest() = 0;
		/*last manifest to be written: usually the VoD one*/
		virtual void finalizeManifest() = 0;

		enum Type {
			Static,
			Live,
			LiveNonBlocking,
		};
		enum AdaptiveStreamingCommonFlags {
			None = 0,
			SegmentsNotOwned     = 1 << 0, //don't touch files
			PresignalNextSegment = 1 << 1, //speculative, allows prefetching on player side
			ForceRealDurations   = 1 << 2
		};

		AdaptiveStreamer(KHost* host, Type type, uint64_t segDurationInMs, const std::string &manifestDir, AdaptiveStreamingCommonFlags flags)
			: m_host(host),
			  type(type),
			  segDurationInMs(segDurationInMs),
			  segDurationIn180k(timescaleToClock(segDurationInMs, 1000)),
			  manifestDir(manifestDir),
			  flags(flags) {
			if ((flags & ForceRealDurations) && !segDurationInMs)
				throw error("Inconsistent parameters: ForceRealDurations flag requires a non-null segment duration.");
			if (!manifestDir.empty() && (flags & SegmentsNotOwned))
				throw error(format("Inconsistent parameters: manifestDir (%s) should be empty when segments are not owned.", manifestDir));
			addInput();
			outputSegments = addOutput();
			outputManifest = addOutput();
		}

		void process() override {
			if (!wasInit && (startTimeInMs==(uint64_t)-1)) {
				startTimeInMs = (uint64_t)-2;

				for (int i = 0; i < numInputs(); ++i)
					qualities.push_back(make_unique<Quality>());

				wasInit = true;
			}

			while(schedule()) { }
		}

		void flush() override {
			while(schedule()) { }
			endOfStream();
		}

		static std::string getCommonPrefixAudio(size_t index) {
			return format("a_%s", index);
		}
		static std::string getCommonPrefixVideo(size_t index, Resolution res) {
			return format("v_%s_%sx%s", index, res.width, res.height);
		}
		static std::string getCommonPrefixSubtitle(size_t index) {
			return format("s_%s", index);
		}

	protected:
		KHost* const m_host;

		const Type type;
		uint64_t startTimeInMs = -1, totalDurationInMs = 0;
		const uint64_t segDurationInMs;
		const uint64_t segDurationIn180k;
		const std::string manifestDir;
		const AdaptiveStreamingCommonFlags flags;
		std::vector<std::unique_ptr<Quality>> qualities;
		OutputDefault* outputSegments;
		OutputDefault* outputManifest;

		bool wasInit = false;

		void processInitSegment(Quality const * const quality, size_t index) {
			auto const &meta = quality->getMeta();
			switch (meta->type) {
			case AUDIO_PKT: case VIDEO_PKT: case SUBTITLE_PKT: {
				auto out = clone(quality->lastData);
				std::string initFn = safe_cast<const MetadataFile>(quality->lastData->getMetadata())->filename;
				if (initFn.empty()) {
					initFn = format("%s%s", manifestDir, getInitName(quality, index));
				} else if (!(flags & SegmentsNotOwned)) {
					auto const dst = format("%s%s", manifestDir, getInitName(quality, index));
					initFn = dst;
				}

				auto metaFn = make_shared<MetadataFile>(SEGMENT);
				metaFn->filename = initFn;
				metaFn->mimeType = meta->mimeType;
				metaFn->codecName = meta->codecName;
				metaFn->durationIn180k = meta->durationIn180k;
				metaFn->filesize = meta->filesize;
				metaFn->latencyIn180k = meta->latencyIn180k;
				metaFn->startsWithRAP = meta->startsWithRAP;

				out->setMetadata(metaFn);
				out->setMediaTime(totalDurationInMs, 1000);
				outputSegments->post(out);
				break;
			}
			default: break;
			}
		}

		std::string getInitName(Quality const * const quality, size_t index) const {
			switch (quality->getMeta()->type) {
			case AUDIO_PKT:
			case VIDEO_PKT:
			case SUBTITLE_PKT:
				return format("%s-init.mp4", getPrefix(quality, index));
			default: return "";
			}
		}

		std::string getPrefix(Quality const * const quality, size_t index) const {
			switch (quality->getMeta()->type) {
			case AUDIO_PKT:    return format("%s%s", quality->prefix, getCommonPrefixAudio(index));
			case VIDEO_PKT:    return format("%s%s", quality->prefix, getCommonPrefixVideo(index, quality->getMeta()->resolution));
			case SUBTITLE_PKT: return format("%s%s", quality->prefix, getCommonPrefixSubtitle(index));
			default: return "";
			}
		}

		std::string getSegmentName(Quality const * const quality, size_t index, const std::string &segmentNumSymbol) const {
			switch (quality->getMeta()->type) {
			case AUDIO_PKT:
			case VIDEO_PKT:
			case SUBTITLE_PKT:
				return format("%s-%s.m4s", getPrefix(quality, index), segmentNumSymbol);
			default: return "";
			}
		}

		uint64_t getCurSegNum() const {
			return (startTimeInMs + totalDurationInMs) / segDurationInMs;
		}

		void endOfStream() {
			if (wasInit) {
				for(auto& in : inputs)
					in->push(nullptr);

				/*final rewrite of MPD in static mode*/
				finalizeManifest();

				wasInit = false;
			}
		}

		std::shared_ptr<DataBase> getPresignalledData(uint64_t size, Data &data, bool EOS) {
			if (!(flags & PresignalNextSegment)) {
				return clone(data);
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
				auto out = outputSegments->allocData<DataRaw>(0);
				out->buffer->resize(headerSize);
				memcpy(out->buffer->data().ptr, mp4StaticHeader, headerSize);
				return out;
			} else if (data->data().len >= headerSize && !memcmp(data->data().ptr, mp4StaticHeader, headerSize)) {
				auto out = outputSegments->allocData<DataRaw>(0);
				auto const size = (size_t)(data->data().len - headerSize);
				out->buffer->resize(size);
				memcpy(out->buffer->data().ptr, data->data().ptr + headerSize, size);
				return out;
			} else {
				assert(data->data().len < 8 || *(uint32_t*)(data->data().ptr + 4) != (uint32_t)0x70797473);
				return clone(data);
			}
		}

		int numInputs() {
			return getNumInputs() - 1;
		}

		Data currData;
		std::vector<uint64_t> curSegDurIn180k;

		bool isComplete(int repIdx) const {
			uint64_t minIncompletSegDur = std::numeric_limits<uint64_t>::max();
			for (size_t idx = 0; idx < curSegDurIn180k.size(); ++idx) {
				auto const &segDur = curSegDurIn180k[idx];
				if ( (segDur < minIncompletSegDur) &&
				    ((segDur < segDurationIn180k) || (!qualities[idx]->getMeta() || !qualities[idx]->getMeta()->EOS))) {
					minIncompletSegDur = segDur;
				}
			}
			return (minIncompletSegDur == std::numeric_limits<uint64_t>::max()) || (curSegDurIn180k[repIdx] > minIncompletSegDur);
		}

		void ensureStartTime() {
			if (startTimeInMs == (uint64_t)-2)
				startTimeInMs = clockToTimescale(currData->get<PresentationTime>().time, 1000);
		}

		void sendLocalData(int repIdx, uint64_t size, bool EOS) {
			ensureStartTime();
			auto out = getPresignalledData(size, currData, EOS);
			if (out) {
				auto const &meta = qualities[repIdx]->getMeta();

				auto metaFn = make_shared<MetadataFile>(SEGMENT);
				metaFn->filename = getSegmentName(qualities[repIdx].get(), repIdx, std::to_string(getCurSegNum()));
				metaFn->mimeType = meta->mimeType;
				metaFn->codecName = meta->codecName;
				metaFn->durationIn180k = meta->durationIn180k;
				metaFn->filesize = size;
				metaFn->latencyIn180k = meta->latencyIn180k;
				metaFn->startsWithRAP = meta->startsWithRAP;
				metaFn->EOS = EOS;

				out->setMetadata(metaFn);
				out->setMediaTime(totalDurationInMs + timescaleToClock(curSegDurIn180k[repIdx], 1000));
				outputSegments->post(out);
			}
		}

		bool segmentReady() {
			for (int i = 0; i < numInputs(); ++i) {
				if (!curSegDurIn180k[0])
					curSegDurIn180k[0] = segDurationIn180k;
			}
			for (int i = 0; i < numInputs(); ++i) {
				if (curSegDurIn180k[i] < segDurationIn180k) {
					return false;
				}
				if (!qualities[i]->getMeta()->EOS) {
					return false;
				}
			}
			return true;
		}

		bool scheduleRepresentation(int repIdx) {
			if (isComplete(repIdx))
				return true;

			if (!inputs[repIdx]->tryPop(currData) || !currData)
				return false;

			qualities[repIdx]->lastData = currData;
			auto const &meta = qualities[repIdx]->getMeta();
			if (!meta)
				throw error(format("Unknown data received on input %s", repIdx));

			if (qualities[repIdx]->prefix.empty())
				qualities[repIdx]->prefix = format("%s/", getPrefix(qualities[repIdx].get(), repIdx));

			auto const curDurIn180k = meta->durationIn180k;
			if (curDurIn180k == 0 && curSegDurIn180k[repIdx] == 0) {
				processInitSegment(qualities[repIdx].get(), repIdx);
				if (flags & PresignalNextSegment)
					sendLocalData(repIdx, 0, false);
				--repIdx;
				currData = nullptr;
				return true;
			}

			// update average bitrate
			if (segDurationInMs && curDurIn180k) {
				auto const numSeg = totalDurationInMs / segDurationInMs;
				qualities[repIdx]->avg_bitrate_in_bps = ((meta->filesize * 8 * IClock::Rate) / meta->durationIn180k + qualities[repIdx]->avg_bitrate_in_bps * numSeg) / (numSeg + 1);
			}

			// update current segment duration
			if (flags & ForceRealDurations) {
				curSegDurIn180k[repIdx] += meta->durationIn180k;
			} else {
				curSegDurIn180k[repIdx] = segDurationIn180k ? segDurationIn180k : meta->durationIn180k;
			}

			if (curSegDurIn180k[repIdx] < segDurationIn180k || !meta->EOS)
				sendLocalData(repIdx, meta->filesize, meta->EOS);

			return true;
		}

		bool schedule() {
			curSegDurIn180k.resize(numInputs());

			for (int repIdx = 0; repIdx < numInputs(); ++repIdx) {
				if(!scheduleRepresentation(repIdx))
					break;
			}

			if (!currData)
				return false; // nothing was done

			ensureStartTime();
			currData = nullptr;

			if (segmentReady()) {
				for (auto &d : curSegDurIn180k)
					d -= segDurationIn180k;

				generateManifest();
				totalDurationInMs += segDurationInMs;
				m_host->log(Info, format("Processes segment (total processed: %ss,", (double)totalDurationInMs / 1000).c_str());
			}

			return true;
		}
};
}

namespace Modules {

namespace {
GF_MPD_AdaptationSet *createAS(uint64_t segDurationInMs, GF_MPD_Period *period, gpacpp::MPD *mpd) {
	auto as = mpd->addAdaptationSet(period);
	GF_SAFEALLOC(as->segment_template, GF_MPD_SegmentTemplate);
	as->segment_template->duration = segDurationInMs;
	as->segment_template->timescale = DASH_TIMESCALE;
	as->segment_template->availability_time_offset = AVAILABILITY_TIMEOFFSET_IN_S;

	//FIXME: arbitrary: should be set by the app, or computed
	as->segment_alignment = GF_TRUE;
	as->bitstream_switching = GF_TRUE;

	return as;
}

std::unique_ptr<gpacpp::MPD> createMPD(bool live, uint32_t minBufferTimeInMs, const std::string &id) {
	return live ?
	    make_unique<gpacpp::MPD>(GF_MPD_TYPE_DYNAMIC, id, g_profiles, minBufferTimeInMs ? minBufferTimeInMs : MIN_BUFFER_TIME_IN_MS_LIVE) :
	    make_unique<gpacpp::MPD>(GF_MPD_TYPE_STATIC, id, g_profiles, minBufferTimeInMs ? minBufferTimeInMs : MIN_BUFFER_TIME_IN_MS_VOD );
}
}


namespace Stream {

AdaptiveStreamer::Type getType(DasherConfig* cfg) {
	if(!cfg->live)
		return AdaptiveStreamer::Static;
	else if(cfg->blocking)
		return AdaptiveStreamer::Live;
	else
		return AdaptiveStreamer::LiveNonBlocking;
}

AdaptiveStreamer::AdaptiveStreamingCommonFlags getFlags(DasherConfig* cfg) {
	uint32_t r = 0;

	if(cfg->segmentsNotOwned)
		r |= AdaptiveStreamer::SegmentsNotOwned;
	if(cfg->presignalNextSegment)
		r |= AdaptiveStreamer::PresignalNextSegment;
	if(cfg->forceRealDurations)
		r |= AdaptiveStreamer::ForceRealDurations;

	return AdaptiveStreamer::AdaptiveStreamingCommonFlags(r);
}

class Dasher : public AdaptiveStreamer {
	public:
		Dasher(KHost* host, DasherConfig* cfg)
			: AdaptiveStreamer(host, getType(cfg), cfg->segDurationInMs, cfg->mpdDir, getFlags(cfg)),
			  m_host(host),
			  mpd(createMPD(cfg->live, cfg->minBufferTimeInMs, cfg->id)), mpdFn(cfg->mpdName), baseURLs(cfg->baseURLs),
			  minUpdatePeriodInMs(cfg->minUpdatePeriodInMs ? cfg->minUpdatePeriodInMs : (segDurationInMs ? cfg->segDurationInMs : 1000)),
			  timeShiftBufferDepthInMs(cfg->timeShiftBufferDepthInMs), initialOffsetInMs(cfg->initialOffsetInMs), useSegmentTimeline(cfg->segDurationInMs == 0) {
			if (useSegmentTimeline && ((flags & PresignalNextSegment) || (flags & SegmentsNotOwned)))
				throw error("Next segment pre-signalling or segments not owned cannot be used with segment timeline.");
		}

	protected:

		KHost* const m_host;
		std::unique_ptr<gpacpp::MPD> mpd;
		const std::string mpdFn;
		const std::vector<std::string> baseURLs;
		const uint64_t minUpdatePeriodInMs;
		const int64_t timeShiftBufferDepthInMs;
		const int64_t initialOffsetInMs;
		const bool useSegmentTimeline = false;

		void ensureManifest() {
			if (!mpd->mpd->availabilityStartTime) {
				mpd->mpd->availabilityStartTime = startTimeInMs + initialOffsetInMs;
				mpd->mpd->time_shift_buffer_depth = (u32)timeShiftBufferDepthInMs;
			}
			mpd->mpd->publishTime = int64_t(getUTC() * 1000);

			if ((type == LiveNonBlocking) && (mpd->mpd->media_presentation_duration == 0)) {
				auto mpdOld = std::move(mpd);
				mpd = createMPD(type, mpdOld->mpd->min_buffer_time, mpdOld->mpd->ID);
				mpd->mpd->availabilityStartTime = mpdOld->mpd->availabilityStartTime;
				mpd->mpd->time_shift_buffer_depth = mpdOld->mpd->time_shift_buffer_depth;
			}

			if (!baseURLs.empty()) {
				auto mpdBaseURL = gf_list_new();
				if (!mpdBaseURL)
					throw error("Can't allocate mpdBaseURL with gf_list_new()");
				for (auto const &baseURL : baseURLs) {
					GF_MPD_BaseURL *url;
					GF_SAFEALLOC(url, GF_MPD_BaseURL);
					url->URL = gf_strdup(baseURL.c_str());
					gf_list_add(mpdBaseURL, url);
				}
				mpd->mpd->base_URLs = mpdBaseURL;
			}

			if (!gf_list_count(mpd->mpd->periods)) {
				auto period = mpd->addPeriod();
				period->ID = gf_strdup(PERIOD_NAME);
				GF_MPD_AdaptationSet *audioAS = nullptr, *videoAS = nullptr;
				for(auto repIdx : getInputs()) {
					GF_MPD_AdaptationSet *as = nullptr;
					auto quality = safe_cast<Quality>(qualities[repIdx].get());
					auto const &meta = quality->getMeta();
					if (!meta) {
						continue;
					}
					switch (meta->type) {
					case AUDIO_PKT: as = audioAS ? audioAS : audioAS = createAS(segDurationInMs, period, mpd.get()); break;
					case VIDEO_PKT: as = videoAS ? videoAS : videoAS = createAS(segDurationInMs, period, mpd.get()); break;
					case SUBTITLE_PKT: as = createAS(segDurationInMs, period, mpd.get()); break;
					default: assert(0);
					}

					auto const repId = format("%s", repIdx);
					auto rep = mpd->addRepresentation(as, repId.c_str(), (u32)quality->avg_bitrate_in_bps);
					quality->rep = rep;
					GF_SAFEALLOC(rep->segment_template, GF_MPD_SegmentTemplate);
					std::string templateName;
					if (useSegmentTimeline) {
						GF_SAFEALLOC(rep->segment_template->segment_timeline, GF_MPD_SegmentTimeline);
						rep->segment_template->segment_timeline->entries = gf_list_new();
						templateName = "$Time$";
						if (mpd->mpd->type == GF_MPD_TYPE_DYNAMIC) {
							mpd->mpd->minimum_update_period = (u32)minUpdatePeriodInMs;
						}
					} else {
						templateName = "$Number$";
						mpd->mpd->minimum_update_period = (u32)minUpdatePeriodInMs * MIN_UPDATE_PERIOD_FACTOR;
						rep->segment_template->start_number = (u32)(startTimeInMs / segDurationInMs);
					}
					rep->mime_type = gf_strdup(meta->mimeType.c_str());
					rep->codecs = gf_strdup(meta->codecName.c_str());
					rep->starts_with_sap = GF_TRUE;
					if (mpd->mpd->type == GF_MPD_TYPE_DYNAMIC && meta->latencyIn180k) {
						rep->segment_template->availability_time_offset = std::max<double>(0.0,  (double)(segDurationInMs - clockToTimescale(meta->latencyIn180k, 1000)) / 1000);
						mpd->mpd->min_buffer_time = (u32)clockToTimescale(meta->latencyIn180k, 1000);
					}
					switch (meta->type) {
					case AUDIO_PKT:
						rep->samplerate = meta->sampleRate;
						break;
					case VIDEO_PKT:
						rep->width = meta->resolution.width;
						rep->height = meta->resolution.height;
						break;
					default: break;
					}

					switch (meta->type) {
					case AUDIO_PKT: case VIDEO_PKT: case SUBTITLE_PKT:
						rep->segment_template->initialization = gf_strdup(getInitName(quality, repIdx).c_str());
						rep->segment_template->media = gf_strdup(getSegmentName(quality, repIdx, templateName).c_str());
						break;
					default: assert(0);
					}
				}
			}
		}

		void postManifest() {
			auto contents = mpd->serialize();

			auto out = outputManifest->allocData<DataRaw>(contents.size());
			auto metadata = make_shared<MetadataFile>(PLAYLIST);
			metadata->filename = manifestDir + mpdFn;
			metadata->durationIn180k = segDurationIn180k;
			metadata->filesize = contents.size();
			out->setMetadata(metadata);
			out->setMediaTime(totalDurationInMs, 1000);
			memcpy(out->buffer->data().ptr, contents.data(), contents.size());
			outputManifest->post(out);
		}

		std::string getPrefixedSegmentName(Quality const * quality, size_t index, u64 segmentNum) const {
			return manifestDir + getSegmentName(quality, index, std::to_string(segmentNum));
		}

		void generateManifest() {
			ensureManifest();

			for(auto i : getInputs()) {
				auto quality = safe_cast<Quality>(qualities[i].get());
				auto const &meta = quality->getMeta();
				if (!meta) {
					continue;
				}
				if (quality->rep->width) { /*video only*/
					quality->rep->starts_with_sap = (quality->rep->starts_with_sap == GF_TRUE && meta->startsWithRAP) ? GF_TRUE : GF_FALSE;
				}

				std::string fn, fnNext;
				if (useSegmentTimeline) {
					auto entries = quality->rep->segment_template->segment_timeline->entries;
					auto const prevEntIdx = gf_list_count(entries);
					GF_MPD_SegmentTimelineEntry *prevEnt = prevEntIdx == 0 ? nullptr : (GF_MPD_SegmentTimelineEntry*)gf_list_get(entries, prevEntIdx-1);
					auto const currDur = clockToTimescale(meta->durationIn180k, 1000);
					uint64_t segTime = 0;
					if (!prevEnt || prevEnt->duration != currDur) {
						auto ent = (GF_MPD_SegmentTimelineEntry*)gf_malloc(sizeof(GF_MPD_SegmentTimelineEntry));
						segTime = ent->start_time = prevEnt ? prevEnt->start_time + prevEnt->duration*(prevEnt->repeat_count+1) : startTimeInMs;
						ent->duration = (u32)currDur;
						ent->repeat_count = 0;
						gf_list_add(entries, ent);
					} else {
						prevEnt->repeat_count++;
						segTime = prevEnt->start_time + prevEnt->duration*(prevEnt->repeat_count);
					}

					fn = getPrefixedSegmentName(quality, i, segTime);
				} else {
					auto n = getCurSegNum();
					fn = getPrefixedSegmentName(quality, i, n);
					if (flags & PresignalNextSegment) {
						fnNext = getPrefixedSegmentName(quality, i, n + 1);
					}
				}

				auto metaFn = make_shared<MetadataFile>(SEGMENT);
				metaFn->filename = fn;
				metaFn->mimeType = meta->mimeType;
				metaFn->codecName= meta->codecName;
				metaFn->durationIn180k= meta->durationIn180k;
				metaFn->filesize= meta->filesize;
				metaFn->latencyIn180k= meta->latencyIn180k;
				metaFn->startsWithRAP= meta->startsWithRAP;

				switch (meta->type) {
				case AUDIO_PKT: metaFn->sampleRate = meta->sampleRate; break;
				case VIDEO_PKT: metaFn->resolution = meta->resolution; break;
				case SUBTITLE_PKT: break;
				default: assert(0);
				}

				if (!fn.empty()) {
					auto out = getPresignalledData(meta->filesize, quality->lastData, true);
					if (!out)
						throw error("Unexpected null pointer detected which getting data.");
					out->setMetadata(metaFn);
					out->setMediaTime(totalDurationInMs, 1000);
					outputSegments->post(out);

					if (!fnNext.empty()) {
						auto out = getPresignalledData(0, quality->lastData, false);
						if (out) {
							auto meta = make_shared<MetadataFile>(*metaFn);
							meta->filename = fnNext;
							meta->EOS = false;
							out->setMetadata(meta);
							out->setMediaTime(totalDurationInMs, 1000);
							outputSegments->post(out);
						}
					}
				}

				if (timeShiftBufferDepthInMs) {
					{
						int64_t totalDuration = 0;
						auto seg = quality->timeshiftSegments.begin();
						while (seg != quality->timeshiftSegments.end()) {
							totalDuration += clockToTimescale(seg->durationIn180k, 1000);
							if (totalDuration > timeShiftBufferDepthInMs) {
								m_host->log(Debug, format( "Delete segment \"%s\".", seg->filename).c_str());
								seg = quality->timeshiftSegments.erase(seg);
							} else {
								++seg;
							}
						}
					}

					if (useSegmentTimeline) {
						int64_t totalDuration = 0;
						auto entries = quality->rep->segment_template->segment_timeline->entries;
						auto idx = gf_list_count(entries);
						while (idx--) {
							auto ent = (GF_MPD_SegmentTimelineEntry*)gf_list_get(quality->rep->segment_template->segment_timeline->entries, idx);
							auto const dur = ent->duration * (ent->repeat_count + 1);
							if (totalDuration > timeShiftBufferDepthInMs) {
								gf_list_rem(entries, idx);
								gf_free(ent);
							}
							totalDuration += dur;
						}
					}

					auto s = Quality::PendingSegment{metaFn->durationIn180k, metaFn->filename};
					quality->timeshiftSegments.emplace(quality->timeshiftSegments.begin(), s);
				}
			}

			if (type != Static) {
				postManifest();
			}
		}

		void finalizeManifest() {
			if (timeShiftBufferDepthInMs) {
				if (!(flags & SegmentsNotOwned)) {
					m_host->log(Info, "Manifest was not rewritten for on-demand and all file are being deleted.");
				}
			} else {
				m_host->log(Info, "Manifest rewritten for on-demand. Media files untouched.");
				mpd->mpd->type = GF_MPD_TYPE_STATIC;
				mpd->mpd->minimum_update_period = 0;
				mpd->mpd->media_presentation_duration = totalDurationInMs;
				totalDurationInMs -= segDurationInMs;
				generateManifest();
				postManifest();
			}
		}

};
}
}

namespace {

IModule* createObject(KHost* host, void* va) {
	auto config = (DasherConfig*)va;
	enforce(host, "MPEG_DASH: host can't be NULL");
	enforce(config, "MPEG_DASH: config can't be NULL");
	return createModule<Stream::Dasher>(host, config).release();
}

auto const registered = Factory::registerModule("MPEG_DASH", &createObject);
}
