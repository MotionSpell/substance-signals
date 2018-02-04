#include "mpeg_dash.hpp"
#include "lib_utils/time.hpp"
#include "../common/libav.hpp"
#ifdef _WIN32
#include <Windows.h>
#endif

#define DASH_TIMESCALE 1000 // /!\ there are some ms already hardcoded from the GPAC calls
#define MOVE_FILE_NUM_RETRY 3
#define MIN_UPDATE_PERIOD_FACTOR 1 //should be 0, but dash.js doesn't support MPDs with no refresh time.

#define MIN_BUFFER_TIME_IN_MS_VOD  3000
#define MIN_BUFFER_TIME_IN_MS_LIVE 2000
#define AVAILABILITY_TIMEOFFSET_IN_S 0.0

static auto const g_profiles = "urn:mpeg:dash:profile:isoff-live:2011, http://dashif.org/guidelines/dash264";

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

std::unique_ptr<gpacpp::MPD> createMPD(Stream::AdaptiveStreamingCommon::Type type, uint32_t minBufferTimeInMs, const std::string &id) {
	return type != Stream::AdaptiveStreamingCommon::Static ?
		uptr(new gpacpp::MPD(GF_MPD_TYPE_DYNAMIC, id, g_profiles, minBufferTimeInMs ? minBufferTimeInMs : MIN_BUFFER_TIME_IN_MS_LIVE)) :
		uptr(new gpacpp::MPD(GF_MPD_TYPE_STATIC , id, g_profiles, minBufferTimeInMs ? minBufferTimeInMs : MIN_BUFFER_TIME_IN_MS_VOD ));
}
}

namespace Stream {

MPEG_DASH::MPEG_DASH(const std::string &mpdDir, const std::string &mpdFilename, Type type, uint64_t segDurationInMs,
	uint64_t timeShiftBufferDepthInMs, uint64_t minUpdatePeriodInMs, uint32_t minBufferTimeInMs,
	const std::vector<std::string> &baseURLs, const std::string &id, int64_t initialOffsetInMs, AdaptiveStreamingCommonFlags flags)
: AdaptiveStreamingCommon(type, segDurationInMs, mpdDir, flags),
  mpd(createMPD(type, minBufferTimeInMs, id)), mpdPath(format("%s%s", mpdDir, mpdFilename)), baseURLs(baseURLs),
  minUpdatePeriodInMs(minUpdatePeriodInMs ? minUpdatePeriodInMs : (segDurationInMs ? segDurationInMs : 1000)),
  timeShiftBufferDepthInMs(timeShiftBufferDepthInMs), initialOffsetInMs(initialOffsetInMs), useSegmentTimeline(segDurationInMs == 0) {
	if (useSegmentTimeline && ((flags & PresignalNextSegment) || (flags & SegmentsNotOwned)))
		throw error("Next segment pre-signalling or segments not owned cannot be used with segment timeline.");
}

MPEG_DASH::~MPEG_DASH() {
	endOfStream();
}

std::unique_ptr<Quality> MPEG_DASH::createQuality() const {
	return uptr<Quality>(safe_cast<Quality>(new DASHQuality));
}

void MPEG_DASH::processInitSegment(Quality const * const quality, size_t index) {
	auto const &meta = quality->getMeta();
	switch (meta->getStreamType()) {
	case AUDIO_PKT: case VIDEO_PKT: case SUBTITLE_PKT: {
		auto out = shptr(new DataBaseRef(quality->lastData));
		auto const initFnSrc = getInitName(quality, index);
		auto const initFnDst = format("%s%s%s", manifestDir, getPeriodID(), initFnSrc);
		moveFile(initFnSrc, initFnDst);
		out->setMetadata(std::make_shared<MetadataFile>(initFnDst, SEGMENT, meta->getMimeType(), meta->getCodecName(), meta->getDuration(), meta->getSize(), meta->getLatency(), meta->getStartsWithRAP(), true));
		outputSegments->emit(out);
		break;
	}
	default: break;
	}
}

void MPEG_DASH::ensureManifest() {
	if (!mpd->mpd->availabilityStartTime) {
		mpd->mpd->availabilityStartTime = startTimeInMs + initialOffsetInMs;
		mpd->mpd->time_shift_buffer_depth = (u32)timeShiftBufferDepthInMs;
	}
	mpd->mpd->publishTime = getUTC().num;

	if ((type == LiveNonBlocking) && (mpd->mpd->media_presentation_duration == 0)) {
		auto mpdOld = std::move(mpd);
		mpd = createMPD(type, mpdOld->mpd->min_buffer_time, mpdOld->mpd->ID);
		mpd->mpd->availabilityStartTime = mpdOld->mpd->availabilityStartTime;
		mpd->mpd->time_shift_buffer_depth = mpdOld->mpd->time_shift_buffer_depth;
	}

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

	if (!gf_list_count(mpd->mpd->periods)) {
		auto period = mpd->addPeriod();
		period->ID = gf_strdup(getPeriodID().c_str());
		GF_MPD_AdaptationSet *audioAS = nullptr, *videoAS = nullptr;
		for (size_t i = 0; i < getNumInputs() - 1; ++i) {
			GF_MPD_AdaptationSet *as = nullptr;
			auto quality = safe_cast<DASHQuality>(qualities[i].get());
			auto const &meta = quality->getMeta();
			if (!meta) {
				continue;
			}
			switch (meta->getStreamType()) {
			case AUDIO_PKT: audioAS ? as = audioAS : as = audioAS = createAS(segDurationInMs, period, mpd.get()); break;
			case VIDEO_PKT: videoAS ? as = videoAS : as = videoAS = createAS(segDurationInMs, period, mpd.get()); break;
			case SUBTITLE_PKT: as = createAS(segDurationInMs, period, mpd.get()); break;
			default: assert(0);
			}

			auto const repId = format("%s", i);
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
			rep->mime_type = gf_strdup(meta->getMimeType().c_str());
			rep->codecs = gf_strdup(meta->getCodecName().c_str());
			rep->starts_with_sap = GF_TRUE;
			if (mpd->mpd->type == GF_MPD_TYPE_DYNAMIC && meta->getLatency()) {
				rep->segment_template->availability_time_offset = std::max<double>(0.0,  (double)(segDurationInMs - clockToTimescale(meta->getLatency(), 1000)) / 1000);
				mpd->mpd->min_buffer_time = (u32)clockToTimescale(meta->getLatency(), 1000);
			}
			switch (meta->getStreamType()) {
			case AUDIO_PKT:
				rep->samplerate = meta->sampleRate;
				break;
			case VIDEO_PKT:
				rep->width = meta->resolution[0];
				rep->height = meta->resolution[1];
				break;
			default: break;
			}

			switch (meta->getStreamType()) {
			case AUDIO_PKT: case VIDEO_PKT: case SUBTITLE_PKT:
				rep->segment_template->initialization = gf_strdup(getInitName(quality, i).c_str());
				rep->segment_template->media = gf_strdup(getSegmentName(quality, i, templateName).c_str());
				break;
			default: assert(0);
			}
		}
	}
}

void MPEG_DASH::writeManifest() {
	if (!mpd->write(mpdPath)) {
		log(Warning, "Can't write MPD at %s (1). Check you have sufficient rights.", mpdPath);
	} else {
		auto out = outputManifest->getBuffer(0);
		auto metadata = std::make_shared<MetadataFile>(mpdPath, PLAYLIST, "", "", timescaleToClock(segDurationInMs, 1000), 0, 1, false, true);
		out->setMetadata(metadata);
		outputManifest->emit(out);
	}
}

bool MPEG_DASH::moveFile(const std::string &src, const std::string &dst) const {
	if (!src.empty() && (src != dst)) {
		if (flags & SegmentsNotOwned)
			throw error(format("Segment not owned requires similar filenames (%s != %s)", src, dst));

		int retry = MOVE_FILE_NUM_RETRY + 1;
#ifdef _WIN32
		while (--retry && (MoveFileA(src.c_str(), dst.c_str())) == 0) {
			if (GetLastError() == ERROR_ALREADY_EXISTS) {
				DeleteFileA(dst.c_str());
			}
#else
		while (--retry && (system(format("%s %s %s", "mv", src, dst).c_str())) == 0) {
#endif
			gf_sleep(10);
		}
		if (!retry) {
			return false;
		}
	}
	return true;
}

std::string MPEG_DASH::getPeriodID() const {
	if (flags & SegmentsNotOwned) {
		return "";
	} else {
		return format("p%s_", qualities.size());
	}
}

std::string MPEG_DASH::getPrefixedSegmentName(DASHQuality const * const quality, size_t index, u64 segmentNum) const {
	return manifestDir + getPeriodID() + getSegmentName(quality, index, std::to_string(segmentNum));
}

void MPEG_DASH::generateManifest() {
	ensureManifest();

	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		auto quality = safe_cast<DASHQuality>(qualities[i].get());
		auto const &meta = quality->getMeta();
		if (!meta) {
			continue;
		}
		if (quality->rep->width) { /*video only*/
			quality->rep->starts_with_sap = (quality->rep->starts_with_sap == GF_TRUE && meta->getStartsWithRAP()) ? GF_TRUE : GF_FALSE;
		}

		std::string fn, fnNext;
		if (useSegmentTimeline) {
			auto entries = quality->rep->segment_template->segment_timeline->entries;
			auto const prevEntIdx = gf_list_count(entries);
			GF_MPD_SegmentTimelineEntry *prevEnt = prevEntIdx == 0 ? nullptr : (GF_MPD_SegmentTimelineEntry*)gf_list_get(entries, prevEntIdx-1);
			auto const currDur = clockToTimescale(meta->getDuration(), 1000);
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
		} else if (mpd->mpd->type != GF_MPD_TYPE_STATIC) { //We are live and not exiting
			auto n = getCurSegNum();
			fn = getPrefixedSegmentName(quality, i, n);
			if (flags & PresignalNextSegment) {
				fnNext = getPrefixedSegmentName(quality, i, n + 1);
			}
		}
		auto metaFn = std::make_shared<MetadataFile>(fn, SEGMENT, meta->getMimeType(), meta->getCodecName(), meta->getDuration(), meta->getSize(), meta->getLatency(), meta->getStartsWithRAP(), true);
		switch (meta->getStreamType()) {
		case AUDIO_PKT: metaFn->sampleRate = meta->sampleRate; break;
		case VIDEO_PKT: metaFn->resolution[0] = meta->resolution[0]; metaFn->resolution[1] = meta->resolution[1]; break;
		case SUBTITLE_PKT: break;
		default: assert(0);
		}

		if (!fn.empty()) {
			log(Debug, "Rename segment \"%s\" -> \"%s\".", meta->getFilename(), fn);
			if (!moveFile(meta->getFilename(), fn)) {
				log(Error, "Couldn't rename segment \"%s\" -> \"%s\". You may encounter playback errors.", meta->getFilename(), fn);
			}

			auto out = shptr(new DataBaseRef(quality->lastData));
			out->setMetadata(metaFn);
			outputSegments->emit(out);

			if (!fnNext.empty()) {
				auto out = shptr(new DataBaseRef(quality->lastData));
				out->setMetadata(std::make_shared<MetadataFile>(fnNext, metaFn->getStreamType(), metaFn->getMimeType(), metaFn->getCodecName(), metaFn->getDuration(), 0, metaFn->getLatency(), metaFn->getStartsWithRAP(), false));
				outputSegments->emit(out);
			}
		}

		if (timeShiftBufferDepthInMs) {
			uint64_t timeShiftSegmentsInMs = 0;
			auto seg = quality->timeshiftSegments.begin();
			while (seg != quality->timeshiftSegments.end()) {
				timeShiftSegmentsInMs += clockToTimescale((*seg).file->getDuration(), 1000);
				if (timeShiftSegmentsInMs > timeShiftBufferDepthInMs) {
					log(Debug, "Delete segment \"%s\".", (*seg).file->getFilename());
					if (gf_delete_file((*seg).file->getFilename().c_str()) == GF_OK || (*seg).retry == 0) {
						seg = quality->timeshiftSegments.erase(seg);
					} else {
						log(Warning, "Couldn't delete old segment \"%s\" (retry=%s).", (*seg).file->getFilename(), (*seg).retry);
						(*seg).retry--;
					}
				} else {
					++seg;
				}
			}

			if (useSegmentTimeline) {
				timeShiftSegmentsInMs = 0;
				auto entries = quality->rep->segment_template->segment_timeline->entries;
				auto idx = gf_list_count(entries);
				while (idx--) {
					auto ent = (GF_MPD_SegmentTimelineEntry*)gf_list_get(quality->rep->segment_template->segment_timeline->entries, idx);
					auto const dur = ent->duration * (ent->repeat_count + 1);
					if (timeShiftSegmentsInMs > timeShiftBufferDepthInMs) {
						gf_list_rem(entries, idx);
						gf_free(ent);
					}
					timeShiftSegmentsInMs += dur;
				}
			}

			quality->timeshiftSegments.emplace(quality->timeshiftSegments.begin(), DASHQuality::SegmentToDelete(metaFn));
		}
	}

	if (type != Static) {
		writeManifest();
	}
}

void MPEG_DASH::finalizeManifest() {
	if (mpd->mpd->time_shift_buffer_depth) {
		if (!(flags & SegmentsNotOwned)) {
			log(Info, "Manifest was not rewritten for on-demand and all file are being deleted.");
			if (gf_delete_file(mpdPath.c_str()) != GF_OK) {
				log(Error, "Couldn't delete MPD: \"%s\".", mpdPath);
			}
			for (size_t i = 0; i < getNumInputs() - 1; ++i) {
				auto quality = safe_cast<DASHQuality>(qualities[i].get());
				std::string fn = manifestDir + getInitName(quality, i);
				if (gf_delete_file(fn.c_str()) != GF_OK) {
					log(Error, "Couldn't delete initialization segment \"%s\".", fn);
				}

				for (auto const &seg : quality->timeshiftSegments) {
					if (gf_delete_file(seg.file->getFilename().c_str()) != GF_OK) {
						log(Error, "Couldn't delete media segment \"%s\".", seg.file->getFilename());
					}
				}
			}
		}
	} else {
		log(Info, "Manifest rewritten for on-demand. Media files untouched.");
		mpd->mpd->type = GF_MPD_TYPE_STATIC;
		mpd->mpd->minimum_update_period = 0;
		mpd->mpd->media_presentation_duration = totalDurationInMs;
		totalDurationInMs -= segDurationInMs;
		generateManifest();
		writeManifest();
	}
}

}
}
