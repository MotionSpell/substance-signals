#include "mpeg_dash.hpp"
#include "lib_modules/core/clock.hpp"
#include "../common/libav.hpp"
#ifdef _WIN32
#include <Windows.h>
#endif

#define DASH_TIMESCALE 1000 // /!\ there are some ms already hardcoded, including in AdaptiveStreamingCommon and gf_net_get_utc() results
#define MOVE_FILE_NUM_RETRY 3
#define MIN_UPDATE_PERIOD_FACTOR 1 //should be 0, but dash.js doesn't support MPDs with no refresh time.

#define MIN_BUFFER_TIME_IN_MS_VOD  3000
#define MIN_BUFFER_TIME_IN_MS_LIVE 2000
#define AVAILABILITY_TIMEOFFSET_IN_S 0.0

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

bool moveFileInternal(const std::string &src, const std::string &dst) {
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
	return true;
}
}

namespace Stream {

MPEG_DASH::MPEG_DASH(const std::string &mpdDir, const std::string &mpdFilename, Type type, uint64_t segDurationInMs, uint64_t timeShiftBufferDepthInMs, uint64_t minUpdatePeriodInMs, uint32_t minBufferTimeInMs, const std::vector<std::string> &baseURLs, const std::string &id, int64_t initialOffsetInMs)
	: AdaptiveStreamingCommon(type, segDurationInMs),
	mpd(type == Live ? new gpacpp::MPD(GF_MPD_TYPE_DYNAMIC, id, "http://dashif.org/guidelines/dash264", minBufferTimeInMs ? minBufferTimeInMs : MIN_BUFFER_TIME_IN_MS_LIVE)
		: new gpacpp::MPD(GF_MPD_TYPE_STATIC, id, "http://dashif.org/guidelines/dash264", minBufferTimeInMs ? minBufferTimeInMs : MIN_BUFFER_TIME_IN_MS_VOD)),
	mpdDir(mpdDir), mpdPath(format("%s%s", mpdDir, mpdFilename)), baseURLs(baseURLs),
	minUpdatePeriodInMs(minUpdatePeriodInMs ? minUpdatePeriodInMs : (segDurationInMs ? segDurationInMs : 1000)),
	timeShiftBufferDepthInMs(timeShiftBufferDepthInMs), initialOffsetInMs(initialOffsetInMs), useSegmentTimeline(segDurationInMs == 0) {
}

MPEG_DASH::~MPEG_DASH() {
	endOfStream();
}

std::unique_ptr<Quality> MPEG_DASH::createQuality() const {
	return uptr<Quality>(safe_cast<Quality>(new DASHQuality));
}

void MPEG_DASH::ensureManifest() {
	if (!mpd->mpd->availabilityStartTime) {
		mpd->mpd->availabilityStartTime = startTimeInMs + initialOffsetInMs;
		mpd->mpd->time_shift_buffer_depth = (u32)timeShiftBufferDepthInMs;
	}
	mpd->mpd->publishTime = gf_net_get_utc();

	auto mpdBaseURL = gf_list_new();
	if (!mpdBaseURL)
		throw error("Can't allocate mpdBaseURL with gf_list_new()");
	for (auto &baseURL : baseURLs) {
		GF_MPD_BaseURL *url;
		GF_SAFEALLOC(url, GF_MPD_BaseURL);
		url->URL = gf_strdup(baseURL.c_str());
		gf_list_add(mpdBaseURL, url);
	}
	mpd->mpd->base_URLs = mpdBaseURL;

	if (!gf_list_count(mpd->mpd->periods)) {
		auto period = mpd->addPeriod();
		period->ID = gf_strdup("p0");
		GF_MPD_AdaptationSet *audioAS = nullptr, *videoAS = nullptr;
		for (size_t i = 0; i < getNumInputs() - 1; ++i) {
			GF_MPD_AdaptationSet *as = nullptr;
			auto quality = safe_cast<DASHQuality>(qualities[i].get());
			switch (quality->meta->getStreamType()) {
			case AUDIO_PKT: audioAS ? as = audioAS : as = audioAS = createAS(segDurationInMs, period, mpd.get()); break;
			case VIDEO_PKT: videoAS ? as = videoAS : as = videoAS = createAS(segDurationInMs, period, mpd.get()); break;
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
			rep->mime_type = gf_strdup(quality->meta->getMimeType().c_str());
			rep->codecs = gf_strdup(quality->meta->getCodecName().c_str());
			rep->starts_with_sap = GF_TRUE;
			if (mpd->mpd->type == GF_MPD_TYPE_DYNAMIC && quality->meta->getLatency()) {
				rep->segment_template->availability_time_offset = std::max<double>(0.0,  (double)(segDurationInMs - clockToTimescale(quality->meta->getLatency(), 1000)) / 1000);
				mpd->mpd->min_buffer_time = (u32)clockToTimescale(quality->meta->getLatency(), 1000);
			}

			std::string initFnSrc;
			switch (quality->meta->getStreamType()) {
			case AUDIO_PKT: {
				rep->samplerate = quality->meta->sampleRate;
				rep->segment_template->initialization = gf_strdup("a_$RepresentationID$-init.mp4");
				rep->segment_template->media = gf_strdup(format("a_$RepresentationID$-%s.m4s", templateName).c_str());
				initFnSrc = format("a_%s-init.mp4", repId);
				break;
			}
			case VIDEO_PKT: {
				rep->width = quality->meta->resolution[0];
				rep->height = quality->meta->resolution[1];
				rep->segment_template->initialization = gf_strdup(format("v_$RepresentationID$_%sx%s-init.mp4", rep->width, rep->height).c_str());
				rep->segment_template->media = gf_strdup(format("v_$RepresentationID$_%sx%s-%s.m4s", rep->width, rep->height, templateName).c_str());
				initFnSrc = format("v_%s_%sx%s-init.mp4", repId, rep->width, rep->height);
				break;
			}
			default:
				assert(0);
			}

			switch (quality->meta->getStreamType()) {
			case AUDIO_PKT:
			case VIDEO_PKT: {
				auto out = outputSegments->getBuffer(0);
				auto const initFnDst = format("%s%s", mpdDir, initFnSrc);
				if (!moveFileInternal(initFnSrc, initFnDst)) {
					log(Error, "Couldn't rename init segment \"%s\" -> \"%s\". You may encounter playback errors.", initFnSrc, initFnDst);
				}
				auto metadata = std::make_shared<MetadataFile>(initFnDst, AUDIO_PKT, "", "", 0, 0, 1, false);
				out->setMetadata(metadata);
				outputSegments->emit(out);
				break;
			}
			default: break;
			}
		}
	}
}

void MPEG_DASH::writeManifest() {
	if (!mpd->write(mpdPath)) {
		log(Warning, "Can't write MPD at %s (1). Check you have sufficient rights.", mpdPath);
	} else {
		auto out = outputManifest->getBuffer(0);
		auto metadata = std::make_shared<MetadataFile>(mpdPath, PLAYLIST, "", "", timescaleToClock(segDurationInMs, 1000), 0, 1, false);
		out->setMetadata(metadata);
		outputManifest->emit(out);
	}
}

std::shared_ptr<const MetadataFile> MPEG_DASH::moveFile(const std::shared_ptr<const MetadataFile> src, const std::string &dst) {
	if (src->getFilename() == dst) {
		return src;
	}
	if (!moveFileInternal(src->getFilename(), dst)) {
		log(Error, "Couldn't rename segment \"%s\" -> \"%s\". You may encounter playback errors.", src->getFilename(), dst);
	}

	auto mf = std::make_shared<MetadataFile>(dst, src->getStreamType(), src->getMimeType(), src->getCodecName(), src->getDuration(), src->getSize(), src->getLatency(), src->getStartsWithRAP());
	switch (src->getStreamType()) {
	case AUDIO_PKT: mf->sampleRate = src->sampleRate; break;
	case VIDEO_PKT: mf->resolution[0] = src->resolution[0]; mf->resolution[1] = src->resolution[1]; break;
	default: assert(0);
	}
	return mf;
}

void MPEG_DASH::generateManifest() {
	ensureManifest();

	for (size_t i = 0; i < getNumInputs() - 1; ++i) {
		auto quality = safe_cast<DASHQuality>(qualities[i].get());
		if (quality->rep->width) { /*video only*/
			quality->rep->starts_with_sap = (quality->rep->starts_with_sap == GF_TRUE && quality->meta->getStartsWithRAP()) ? GF_TRUE : GF_FALSE;
		}

		if (useSegmentTimeline) {
			auto entries = quality->rep->segment_template->segment_timeline->entries;
			auto const prevEntIdx = gf_list_count(entries);
			GF_MPD_SegmentTimelineEntry *prevEnt = prevEntIdx == 0 ? nullptr : (GF_MPD_SegmentTimelineEntry*)gf_list_get(entries, prevEntIdx-1);
			auto const currDur = clockToTimescale(quality->meta->getDuration(), 1000);
			uint64_t segTime;
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

			std::string fn;
			switch (quality->meta->getStreamType()) {
			case AUDIO_PKT: fn = format("%sa_%s-%s.m4s", mpdDir, i, segTime); break;
			case VIDEO_PKT: fn = format("%sv_%s_%sx%s-%s.m4s", mpdDir, i, quality->rep->width, quality->rep->height, segTime); break;
			default: assert(0);
			}
			log(Debug, "Rename segment \"%s\" -> \"%s\".", quality->meta->getFilename(), fn);
			quality->meta = moveFile(quality->meta, fn);
		} else {
			auto const n = (startTimeInMs + totalDurationInMs) / segDurationInMs;
			std::string fn;
			switch (quality->meta->getStreamType()) {
			case AUDIO_PKT: fn = format("%sa_%s-%s.m4s", mpdDir, i, n); break;
			case VIDEO_PKT: fn = format("%sv_%s_%sx%s-%s.m4s", mpdDir, i, quality->rep->width, quality->rep->height, n); break;
			default: assert(0);
			}

			quality->meta = moveFile(quality->meta, fn);
		}
		auto out = outputSegments->getBuffer(0);
		out->setMetadata(quality->meta);
		outputSegments->emit(out);

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

			quality->timeshiftSegments.emplace(quality->timeshiftSegments.begin(), DASHQuality::SegmentToDelete(quality->meta));
		}
	}

	if (type == Live) {
		writeManifest();
	}
}

void MPEG_DASH::finalizeManifest() {
	if (mpd->mpd->time_shift_buffer_depth) {
		log(Info, "Manifest was not rewritten for on-demand and all file are being deleted.");
		if (gf_delete_file(mpdPath.c_str()) != GF_OK) {
			log(Error, "Couldn't delete MPD: \"%s\".", mpdPath);
		}
		for (size_t i = 0; i < getNumInputs() - 1; ++i) {
			auto quality = safe_cast<DASHQuality>(qualities[i].get());
			std::string fn;
			switch (quality->meta->getStreamType()) {
			case AUDIO_PKT: fn = format("%sa_%s-init.mp4", mpdDir, i); break;
			case VIDEO_PKT: fn = format("%sv_%s_%sx%s-init.mp4", mpdDir, i, quality->meta->resolution[0], quality->meta->resolution[1]); break;
			default: assert(0);
			}
			if (gf_delete_file(fn.c_str()) != GF_OK) {
				log(Error, "Couldn't delete initialization segment \"%s\".", fn);
			}

			for (auto &seg : quality->timeshiftSegments) {
				if (gf_delete_file(seg.file->getFilename().c_str()) != GF_OK) {
					log(Error, "Couldn't delete media segment \"%s\".", seg.file->getFilename());
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
