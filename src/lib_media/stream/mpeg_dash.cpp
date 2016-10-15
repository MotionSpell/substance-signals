#include "mpeg_dash.hpp"
#include "../common/libav.hpp"

#define DASH_TIMESCALE 1000 //TODO: there are some ms already hardcoded, including in AdaptiveStreamingCommon

#define MIN_BUFFER_TIME_IN_MS_VOD  3000
#define MIN_BUFFER_TIME_IN_MS_LIVE 1000

#define AVAILABILITY_TIMEOFFSET_IN_S 0.0

namespace Modules {

namespace {
GF_MPD_AdaptationSet *createAS(uint64_t segDurationInMs, GF_MPD_Period *period, gpacpp::MPD *mpd) {
	auto as = mpd->addAdaptationSet(period);
	GF_SAFEALLOC(as->segment_template, GF_MPD_SegmentTemplate);
	as->segment_template->duration = segDurationInMs;
	as->segment_template->timescale = DASH_TIMESCALE;
	as->segment_template->start_number = 1;
	as->segment_template->availability_time_offset = AVAILABILITY_TIMEOFFSET_IN_S;

	//FIXME: arbitrary: should be set by the app, or computed
	as->segment_alignment = GF_TRUE;
	as->bitstream_switching = GF_TRUE;

	return as;
}
}

namespace Stream {

MPEG_DASH::MPEG_DASH(const std::string &mpdDir, const std::string &mpdName, Type type, uint64_t segDurationInMs, uint64_t timeShiftBufferDepthInMs)
	: AdaptiveStreamingCommon(type, segDurationInMs),
	  mpd(type == Live ? new gpacpp::MPD(GF_MPD_TYPE_DYNAMIC, MIN_BUFFER_TIME_IN_MS_LIVE)
	  : new gpacpp::MPD(GF_MPD_TYPE_STATIC, MIN_BUFFER_TIME_IN_MS_VOD)),
	    mpdDir(mpdDir), mpdPath(format("%s/%s", mpdDir, mpdName)),
	    useSegmentTimeline(segDurationInMs == 0), timeShiftBufferDepthInMs(timeShiftBufferDepthInMs) {
}

MPEG_DASH::~MPEG_DASH() {
	endOfStream();
}

std::unique_ptr<Quality> MPEG_DASH::createQuality() const {
	return uptr<Quality>(safe_cast<Quality>(new DASHQuality));
}

void MPEG_DASH::ensureManifest() {
	if (!mpd->mpd->availabilityStartTime) {
		mpd->mpd->availabilityStartTime = startTimeInMs;
		mpd->mpd->time_shift_buffer_depth = (u32)timeShiftBufferDepthInMs;
	}
	mpd->mpd->publishTime = gf_net_get_utc();

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
			rep->segment_template->start_number = 1;
			std::string templateName = "$Number$";
			if (useSegmentTimeline) {
				GF_SAFEALLOC(rep->segment_template->segment_timeline, GF_MPD_SegmentTimeline);
				rep->segment_template->segment_timeline->entries = gf_list_new();
				templateName = "$Time$";
			}
			rep->mime_type = gf_strdup(quality->meta->getMimeType().c_str());
			rep->codecs = gf_strdup(quality->meta->getCodecName().c_str());
			rep->starts_with_sap = GF_TRUE;
			if (quality->meta->getLatency()) {
				rep->segment_template->availability_time_offset = std::max<double>(0.0, segDurationInMs * (1.0 - (double)quality->meta->getLatency() / Clock::Rate));
			}
			switch (quality->meta->getStreamType()) {
			case AUDIO_PKT: {
				rep->samplerate = quality->meta->sampleRate;
				rep->segment_template->initialization = gf_strdup(format("audio_$RepresentationID$.mp4").c_str());
				rep->segment_template->media = gf_strdup(format("audio_$RepresentationID$.mp4_%s.m4s", templateName).c_str());

				auto out = outputSegment->getBuffer(0);
				auto metadata = std::make_shared<MetadataFile>(format("%s/audio_%s.mp4", mpdDir, repId), AUDIO_PKT, "", "", 0, 0, 1, false);
				out->setMetadata(metadata);
				outputSegment->emit(out);
				break;
			}
			case VIDEO_PKT: {
				rep->width = quality->meta->resolution[0];
				rep->height = quality->meta->resolution[1];
				rep->segment_template->initialization = gf_strdup(format("video_$RepresentationID$_%sx%s.mp4", rep->width, rep->height).c_str());
				rep->segment_template->media = gf_strdup(format("video_$RepresentationID$_%sx%s.mp4_%s.m4s", rep->width, rep->height, templateName).c_str());

				auto out = outputSegment->getBuffer(0);
				auto metadata = std::make_shared<MetadataFile>(format("%s/video_%s_%sx%s.mp4", mpdDir, repId, rep->width, rep->height), VIDEO_PKT, "", "", 0, 0, 1, false);
				out->setMetadata(metadata);
				outputSegment->emit(out);
				break;
			}
			default:
				assert(0);
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
				segTime = ent->start_time = prevEnt ? prevEnt->start_time + prevEnt->duration*(prevEnt->repeat_count+1) : 0;
				ent->duration = (u32)currDur;
				ent->repeat_count = 0;
				gf_list_add(entries, ent);
			} else {
				prevEnt->repeat_count++;
				segTime = prevEnt->start_time + prevEnt->duration*(prevEnt->repeat_count);
			}

			std::string fn;
			switch (quality->meta->getStreamType()) {
			case AUDIO_PKT: fn = format("audio_%s.mp4_%s.m4s", i, segTime); break;
			case VIDEO_PKT: fn = format("video_%s_%sx%s.mp4_%s.m4s", i, quality->rep->width, quality->rep->height, segTime); break;
			default: assert(0);
			}
			log(Debug, "Rename segment \"%s\" -> \"%s\".", quality->meta->getFilename(), fn);
			int retry = 20;
			while (retry-- && (gf_move_file(quality->meta->getFilename().c_str(), fn.c_str()) != GF_OK)) {
				gf_sleep(10);
			}
			if (!retry) {
				log(Error, "Couldn't rename segment \"%s\" -> \"%s\". You may encounter playback errors.", quality->meta->getFilename(), fn);
			}
			quality->meta = std::make_shared<MetadataFile>(fn, quality->meta->getStreamType(), quality->meta->getMimeType(), quality->meta->getCodecName(), quality->meta->getDuration(), quality->meta->getSize(), quality->meta->getLatency(), quality->meta->getStartsWithRAP());
		}

		if (timeShiftBufferDepthInMs) {
			uint64_t timeShiftSegmentsInMs = 0;
			auto seg = quality->timeshiftSegments.begin();
			while (seg != quality->timeshiftSegments.end()) {
				timeShiftSegmentsInMs += clockToTimescale((*seg)->getDuration(), 1000);
				if (timeShiftSegmentsInMs > timeShiftBufferDepthInMs) {
					log(Warning, "Delete segment \"%s\".", (*seg)->getFilename());
					if (gf_delete_file((*seg)->getFilename().c_str()) != GF_OK) {
						log(Error, "Couldn't delete old segment \"%s\".", (*seg)->getFilename());
					}
					seg = quality->timeshiftSegments.erase(seg);
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

			quality->timeshiftSegments.emplace(quality->timeshiftSegments.begin(), quality->meta);
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
			case AUDIO_PKT: fn = format("audio_%s.mp4", i); break;
			case VIDEO_PKT: fn = format("video_%s_%sx%s.mp4", i, quality->meta->resolution[0], quality->meta->resolution[1]); break;
			default: assert(0);
			}
			if (gf_delete_file(fn.c_str()) != GF_OK) {
				log(Error, "Couldn't delete initialization segment \"%s\".", fn);
			}

			for (auto &seg : quality->timeshiftSegments) {
				if (gf_delete_file(seg->getFilename().c_str()) != GF_OK) {
					log(Error, "Couldn't delete media segment \"%s\".", seg->getFilename());
				}
			}
		}
	} else {
		log(Info, "Manifest rewritten for on-demand. Media files untouched.");
		mpd->mpd->type = GF_MPD_TYPE_STATIC;
		mpd->mpd->minimum_update_period = 0;
		mpd->mpd->media_presentation_duration = totalDurationInMs;
		generateManifest();
		writeManifest();
	}
}

}
}
