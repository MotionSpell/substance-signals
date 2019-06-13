// Copyright 2013 - Romain Bouqueau, Samurai Akihiro, Motion Spell S.A.R.L.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <cstdint>
#include <memory>

extern "C" {
#include <gpac/tools.h>
#include <gpac/isomedia.h>
#include <gpac/media_tools.h>
#include <gpac/internal/mpd.h>
}

//#define GPAC_MEM_TRACKER

extern const char *g_version;

namespace gpacpp {

//------------------------------------------------
// GPAC error
//------------------------------------------------
class Error : public std::exception {
	public:
		Error(char const* msg, GF_Err error) throw() :  error_(error), msg_(msg) {
			msg_ += ": ";
			msg_ += gf_error_to_string(error_);
		}

		char const* what() const throw() {
			return msg_.c_str();
		}

		const GF_Err error_;
	private:
		std::string msg_;
};

#ifndef GPAC_DISABLE_ISOM

//------------------------------------------------
// wrapper for GF_ISOSample
//------------------------------------------------
class IsoSample : public GF_ISOSample {
	public:
		IsoSample() {
			data = NULL;
			dataLength = 0;
			DTS = 0;
			CTS_Offset = 0;
			IsRAP = RAP_NO;
		}
		IsoSample(GF_ISOSample* pOther) {
			*((GF_ISOSample*)this) = *pOther;
			gf_free(pOther);
		}
		~IsoSample() {
			if (ownsData)
				gf_free(data);
		}
		void setDataOwnership(bool ownsData) {
			this->ownsData = ownsData;
		}

		bool isRap() const {
			return this->IsRAP == RAP;
		}

	private:
		IsoSample const& operator=(IsoSample const&) = delete;
		bool ownsData = true;
};

//------------------------------------------------
// wrapper for GF_ISOFile
//------------------------------------------------
class IsoFile {
	public:
		IsoFile(std::string const& url) {
			u64 missingBytes;
			GF_Err e = gf_isom_open_progressive(url.c_str(), 0, 0, &movie_, &missingBytes);
			if (e != GF_OK && e != GF_ISOM_INCOMPLETE_FILE)
				throw Error("Can't open fragmented MP4 file", e);
		}

		IsoFile(GF_ISOFile* movie) : movie_(movie) {
		}

		~IsoFile() {
			gf_isom_close(movie_);
		}

		IsoFile const& operator=(IsoFile const&) = delete;

		void setSingleMoofMode(bool enable) {
			gf_isom_set_single_moof_mode(movie_, (Bool)enable);
		}

		bool isFragmented() const {
			return gf_isom_is_fragmented(movie_);
		}

		uint64_t refreshFragmented(std::string const& url) {
			uint64_t missingBytes;
			GF_Err e = gf_isom_refresh_fragmented(movie_, &missingBytes, url.c_str());
			if (e != GF_OK && e != GF_ISOM_INCOMPLETE_FILE)
				throw Error("Can't refresh fragmented MP4", e);
			return missingBytes;
		}

		uint32_t getTrackId(int i) const {
			return gf_isom_get_track_id(movie_, i);
		}

		uint32_t getTrackById(uint32_t id) const {
			auto const number = gf_isom_get_track_by_id(movie_, id);
			if (number == 0)
				throw Error("Can't find track ID", GF_BAD_PARAM);
			return number;
		}

		uint32_t getSampleCount(int trackNumber) const {
			return gf_isom_get_sample_count(movie_, trackNumber);
		}

		std::unique_ptr<IsoSample> getSample(int trackNumber, int sampleIndex, int& descriptorIndex) const {
			GF_ISOSample* sample = gf_isom_get_sample(movie_, trackNumber, sampleIndex, (u32*)&descriptorIndex);
			if (!sample) {
				auto msg = "Sample with index " + std::to_string(sampleIndex) + " does not exist (trackNumber=" + std::to_string(trackNumber) + ")";
				throw Error(msg.c_str(), gf_isom_last_error(movie_));
			}
			return std::unique_ptr<IsoSample>(new IsoSample(sample));
		}

		uint32_t getMediaTimescale(int trackNumber) const {
			return gf_isom_get_media_timescale(movie_, trackNumber);
		}

		int64_t getDTSOffset(int trackNumber) const {
			s64 DTSOffset = 0;
			gf_isom_get_edit_list_type(movie_, trackNumber, &DTSOffset);
			return DTSOffset;
		}

		std::shared_ptr<GF_DecoderConfig> getDecoderConfig(int trackHandle, int descriptorIndex) const {
			return std::shared_ptr<GF_DecoderConfig>(
			        gf_isom_get_decoder_config(movie_, trackHandle, descriptorIndex),
			        &freeDescriptor
			    );
		}

		static void freeDescriptor(GF_DecoderConfig* desc) {
			gf_odf_desc_del((GF_Descriptor*)desc);
		}

		void resetTables(bool flag) {
			gf_isom_reset_tables(movie_, (Bool)flag);
		}

		void resetDataOffset(uint64_t& newBufferStart) {
			GF_Err e = gf_isom_reset_data_offset(movie_, &newBufferStart);
			if (e != GF_OK)
				throw gpacpp::Error("Could not release data", e);
		}

		uint64_t getMissingBytes(int trackNumber) {
			return gf_isom_get_missing_bytes(movie_, trackNumber);
		}

		GF_Err lastError() const {
			return gf_isom_last_error(movie_);
		}

	private:
		GF_ISOFile* movie_;
};

#endif /*GPAC_DISABLE_ISOM*/

#ifndef GPAC_DISABLE_CORE_TOOLS

//------------------------------------------------
// wrapper for GF_MPD
//------------------------------------------------
class MPD {
	public:
		MPD(GF_MPD_Type type, const std::string &id, const std::string &profiles, u32 minBufferTime) {
			mpd = gf_mpd_new();
			mpd->xml_namespace = "urn:mpeg:dash:schema:mpd:2011";
			mpd->type = type;
			mpd->min_buffer_time = minBufferTime;
			mpd->profiles = gf_strdup(profiles.c_str());
			if (type == GF_MPD_TYPE_DYNAMIC) {
				mpd->minimum_update_period = 1000;
			}

			mpd->ID = gf_strdup(id.c_str());
			mpd->program_infos = gf_list_new();
			GF_MPD_ProgramInfo *info;
			GF_SAFEALLOC(info, GF_MPD_ProgramInfo);
			info->more_info_url = gf_strdup("http://signals.gpac-licensing.com");
			//info->title = gf_strdup();
			//info->source = gf_strdup(format("Generated from URL \"%s\"", ).c_str());
			info->copyright = gf_strdup((std::string("Generated by GPAC Signals/") + g_version).c_str());
			gf_list_add(mpd->program_infos, info);
			//mpd->availabilityStartTime
			//mpd->availabilityEndTime
			//mpd->publishTime
			//mpd->media_presentation_duration
			//mpd->time_shift_buffer_depth
			//mpd->suggested_presentation_delay
			//mpd->max_segment_duration
			//mpd->max_subsegment_duration
			//mpd->attributes) gf_mpd_extensible_print_attr(out, (GF_MPD_ExtensibleVirtual*)mpd);
			//mpd->children) gf_mpd_extensible_print_nodes(out, (GF_MPD_ExtensibleVirtual*)mpd);
			//mpd->base_URLs
			//mpd->locations
		}

		virtual ~MPD() {
			gf_mpd_del(mpd);
		}

		MPD(MPD const&) = delete;
		void operator=(MPD const&) = delete;

		MPD(MPD&& other) {
			mpd = other.mpd;
			other.mpd = nullptr;
		}

		void operator=(MPD&& other) {
			gf_mpd_del(mpd);
			mpd = other.mpd;
			other.mpd = nullptr;
		}

		auto operator->() {
			return mpd;
		}

		std::string serialize() const {
			auto fp = std::shared_ptr<FILE>(gf_temp_file_new(nullptr), &gf_fclose);
			if(!fp)
				throw Error("[MPEG-DASH MPD] Can't serialize manifest (2)", GF_IO_ERR);

			GF_Err e = gf_mpd_write(mpd, fp.get());
			if (e != GF_OK)
				throw Error("[MPEG-DASH MPD] Can't serialize manifest (1)", e);

			std::string r;
			r.resize(gf_ftell(fp.get()));
			gf_fseek(fp.get(), 0, SEEK_SET);
			auto nread = gf_fread(&r[0], 1, r.size(), fp.get());
			if(nread != r.size())
				throw Error("[MPEG-DASH MPD] Can't read serialized manifest", GF_IO_ERR);

			return r;
		}

		GF_MPD_Representation* addRepresentation(GF_MPD_AdaptationSet *as, const char *id, u32 bandwidth) {
			GF_MPD_Representation *rep;
			GF_SAFEALLOC(rep, GF_MPD_Representation);

			//GF_MPD_COMMON_ATTRIBUTES_ELEMENTS
			//
			rep->id = gf_strdup(id);
			rep->bandwidth = bandwidth;
			//u32 quality_ranking;
			//char *dependency_id;
			//char *media_stream_structure_id;
			//
			//GF_List *base_URLs;
			//GF_MPD_SegmentBase *segment_base;
			//GF_MPD_SegmentList *segment_list;
			//GF_MPD_SegmentTemplate *segment_template;
			//
			//GF_List *sub_representations;
			//
			///*index of the next enhancement representation plus 1, 0 is reserved in case of the highest representation*/
			//u32 enhancement_rep_index_plus_one;
			//
			///*GPAC playback implementation*/
			//GF_DASH_RepresentationPlayback playback;
			//u32 m3u8_media_seq_min, m3u8_media_seq_max;

			if (!as->representations)
				as->representations = gf_list_new();
			gf_list_add(as->representations, rep);
			return rep;
		}

		GF_MPD_AdaptationSet* addAdaptationSet(GF_MPD_Period *period) {
			GF_MPD_AdaptationSet *as;
			GF_SAFEALLOC(as, GF_MPD_AdaptationSet);

			//GF_MPD_COMMON_ATTRIBUTES_ELEMENTS

			//u32 id;
			///*default value is -1: not set in MPD*/
			as->group = -1;
			//
			//char *lang;
			//char *content_type;
			//GF_MPD_Fractional *par;
			//u32 min_bandwidth;
			//u32 max_bandwidth;
			//u32 min_width;
			//u32 max_width;
			//u32 min_height;
			//u32 max_height;
			//u32 min_framerate;
			//u32 max_framerate;
			//Bool segment_alignment;
			//Bool bitstream_switching;
			//Bool subsegment_alignment;
			//Bool subsegment_starts_with_sap;
			//
			//GF_List *accessibility;
			//GF_List *role;
			//GF_List *rating;
			//GF_List *viewpoint;
			//GF_List *content_component;
			//
			//GF_List *base_URLs;
			//GF_MPD_SegmentBase *segment_base;
			//GF_MPD_SegmentList *segment_list;
			//GF_MPD_SegmentTemplate *segment_template;
			//
			//char *xlink_href;
			//Bool xlink_actuate_on_load;

			if (!period->adaptation_sets)
				period->adaptation_sets = gf_list_new();
			gf_list_add(period->adaptation_sets, as);
			return as;
		}

		GF_MPD_Period* addPeriod() {
			GF_MPD_Period *period;
			GF_SAFEALLOC(period, GF_MPD_Period);

			//char *ID;
			period->start = 0;
			//u32 duration; /* expressed in ms*/
			//Bool bitstream_switching;

			//GF_List *base_URLs;
			//GF_MPD_SegmentBase *segment_base;
			//GF_MPD_SegmentList *segment_list;
			//GF_MPD_SegmentTemplate *segment_template;

			//GF_List *subsets;
			//char *xlink_href;
			//Bool xlink_actuate_on_load;

			if (!mpd->periods)
				mpd->periods = gf_list_new();
			gf_list_add(mpd->periods, period);
			return period;
		}

		GF_MPD *mpd;

	private:
		bool minimalCheck() const {
			if (!mpd->min_buffer_time)
				return false;

			if (mpd->type == GF_MPD_TYPE_STATIC) {
				if (!mpd->media_presentation_duration)
					return false;
			} else if (mpd->type == GF_MPD_TYPE_DYNAMIC) {
				if (!mpd->availabilityStartTime)
					return false;
			} else {
				//unknown mpd type
				return false;
			}

			//check we have at least one AS with one representation
			if (!gf_list_count(mpd->periods))
				return false;
			GF_MPD_Period *period = (GF_MPD_Period*)gf_list_get(mpd->periods, 0);
			if (!gf_list_count(period->adaptation_sets))
				return false;
			GF_MPD_AdaptationSet *as = (GF_MPD_AdaptationSet*)gf_list_get(period->adaptation_sets, 0);
			if (!gf_list_count(as->representations))
				return false;

			return true;
		}
};

#endif /*GPAC_DISABLE_CORE_TOOLS*/

}
