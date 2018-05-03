#pragma once

#include "lib_utils/format.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <stdexcept>

extern "C" {
#include <gpac/tools.h>
#include <gpac/isomedia.h>
#include <gpac/thread.h>
#include <gpac/media_tools.h>
#include <gpac/download.h>
#include <gpac/dash.h>
#include <gpac/internal/mpd.h>
#include <gpac/mpegts.h>
}

//#define GPAC_MEM_TRACKER

extern const char *g_version;

namespace gpacpp {

//------------------------------------------------
// GPAC error
//------------------------------------------------
class Error : public std::exception {
	public:
		Error(char const* msg, GF_Err error) throw() : msg_(msg), error_(error) {
			msg_ += ": ";
			msg_ += gf_error_to_string(error_);
		}

		~Error() throw() {
		}

		char const* what() const throw() {
			return msg_.c_str();
		}

		std::string msg_;
		const GF_Err error_;

	private:
		Error& operator= (const Error&) = delete;
};

//------------------------------------------------
// wrapper for GPAC init
//------------------------------------------------
class Init {
	public:
		Init(/*bool memTracker = false, */GF_LOG_Tool globalLogTools = GF_LOG_ALL, GF_LOG_Level globalLogLevel = GF_LOG_WARNING) {
#ifdef GPAC_MEM_TRACKER
			gf_sys_init(GF_MemTrackerBackTrace);
#else
			gf_sys_init(GF_MemTrackerNone);
#endif
			gf_log_set_tool_level(globalLogTools, globalLogLevel);
		}

		~Init() {
			gf_sys_close();
		}
};

//------------------------------------------------
// wrapper for GF_Config
//------------------------------------------------
class Config {
	public:
		Config(const char *filePath, const char *fileName) {
			cfg = gf_cfg_new(filePath, fileName);
		}

		~Config() {
			gf_cfg_del(cfg);
		}

		GF_Config* get() const {
			return cfg;
		}

		void setKey(const char *secName, const char *keyName, const char *keyValue) {
			GF_Err e = gf_cfg_set_key(cfg, secName, keyName, keyValue);
			if (e < 0)
				throw Error(format("[GPACPP] Cannot set config (%s) key [%s] %s=%s", cfg, secName, keyName, keyValue).c_str(), e);
		}

		const char* getKey(const char *secName, const char *keyName) const {
			return gf_cfg_get_key(cfg, secName, keyName);
		}

		Config const& operator=(Config const&) = delete;

	private:
		GF_Config *cfg;
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
			GF_ISOSample* pMe = this;
			memcpy(pMe, pOther, sizeof(*pOther));
			gf_free(pOther);
		}
		~IsoSample() {
			if (ownsData)
				gf_free(data);
		}
		void setDataOwnership(bool ownsData) {
			this->ownsData = ownsData;
		}

	private:
		IsoSample const& operator=(IsoSample const&) = delete;
		bool ownsData = true;
};

//------------------------------------------------
// wrapper for GF_ISOFile
//------------------------------------------------
class IsoFile : public Init {
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

		void refreshFragmented(uint64_t& missingBytes, std::string const& url) {
			GF_Err e = gf_isom_refresh_fragmented(movie_, &missingBytes, url.c_str());
			if (e != GF_OK && e != GF_ISOM_INCOMPLETE_FILE)
				throw Error("Can't refreshing fragmented MP4", e);
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
			if (!sample)
				throw Error("Sample does not exist", gf_isom_last_error(movie_));
			return std::unique_ptr<IsoSample>(new IsoSample(sample));
		}

		uint32_t getMediaTimescale(int trackNumber) const {
			return gf_isom_get_media_timescale(movie_, trackNumber);
		}

		int64_t getDTSOffet(int trackNumber) const {
			s64 DTSOffset = 0;
			gf_isom_get_edit_list_type(movie_, trackNumber, &DTSOffset);
			return DTSOffset;
		}

		GF_DecoderConfig* getDecoderConfig(int trackHandle, int descriptorIndex) const {
			return gf_isom_get_decoder_config(movie_, trackHandle, descriptorIndex);
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

//------------------------------------------------
// wrapper for GF_DownloadManager
//------------------------------------------------
class DownloadManager : public Init {
	public:
		DownloadManager(const Config *cfg) {
			dm = gf_dm_new(cfg->get());
		}

		~DownloadManager() {
			gf_dm_del(dm);
		}

		GF_DownloadManager* get() const {
			return dm;
		}

		GF_DownloadManager const& operator=(GF_DownloadManager const&) = delete;

	private:
		GF_DownloadManager *dm;
};

#ifndef GPAC_DISABLE_DASH_CLIENT

//------------------------------------------------
// wrapper for GF_DashClient
//------------------------------------------------
class DashClient : public Init {
	public:
		DashClient(GF_DASHFileIO *dashIO, Config *cfg) : dashIO(dashIO), cfg(cfg), dm(new DownloadManager(cfg)) {
			dashClient = gf_dash_new(this->dashIO.get(), 10, 0, GF_FALSE, GF_TRUE, GF_DASH_SELECT_QUALITY_LOWEST, GF_FALSE, 0);
			if (dashClient == nullptr)
				throw std::runtime_error("Can't create DASH Client");
			state = Init;
		}

		void start(std::string const& url) {
			assert(state == Init);
			GF_Err e = gf_dash_open(dashClient, url.c_str());
			if (e)
				throw Error(format("[MPEG-DASH Client] Can't open URL %s", url).c_str(), e);
			state = Started;
		}

		void stop() {
			gf_dash_close(dashClient);
			state = Stopped;
		}

		~DashClient() {
			if (state == Started)
				stop();
			gf_dash_del(dashClient);
		}

		GF_DashClient* get() const {
			return dashClient;
		}

		DownloadManager* downloader() {
			return dm.get();
		}

	private:
		enum State {
			Init,
			Started,
			Stopped
		};

		State state;
		std::unique_ptr<GF_DASHFileIO> dashIO;
		std::unique_ptr<Config> cfg;
		std::unique_ptr<DownloadManager> dm;
		GF_DashClient *dashClient;
};

#endif /*GPAC_DISABLE_DASH_CLIENT*/

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
			info->copyright = gf_strdup(format("Generated by GPAC Signals/%s", g_version).c_str());
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

		bool write(std::string const& url) const {
			if (!minimalCheck())
				return false;

			GF_Err e = gf_mpd_write_file(mpd, url.c_str());
			if (e != GF_OK)
				throw Error(format("[MPEG-DASH MPD] Can't write file %s", url).c_str(), e);

			return true;
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

#ifndef GPAC_DISABLE_MPEG2TS_MUX
//------------------------------------------------
// wrapper for GF_M2TS_Mux
//------------------------------------------------
class M2TSMux : public Init {
	public:
		M2TSMux(bool real_time, unsigned mux_rate, unsigned pcr_ms = 100, int64_t pcr_init_val = -1) {
			const GF_M2TS_PackMode single_au_pes = GF_M2TS_PACK_AUDIO_ONLY;
			const int pcrOffset = 0;
			const int curPid = 100;

			muxer = gf_m2ts_mux_new(mux_rate, GF_M2TS_PSI_DEFAULT_REFRESH_RATE, real_time == true ? GF_TRUE : GF_FALSE);
			if (!muxer)
				throw std::runtime_error("[GPACMuxMPEG2TS] Could not create the muxer.");

			gf_m2ts_mux_use_single_au_pes_mode(muxer, single_au_pes);
			if (pcr_init_val >= 0)
				gf_m2ts_mux_set_initial_pcr(muxer, (u64)pcr_init_val);

			gf_m2ts_mux_set_pcr_max_interval(muxer, pcr_ms);
			program = gf_m2ts_mux_program_add(muxer, 1, curPid, GF_M2TS_PSI_DEFAULT_REFRESH_RATE, pcrOffset, GF_FALSE, 0, GF_TRUE);
			if (!program)
				throw std::runtime_error("[GPACMuxMPEG2TS] Could not create the muxer.");
		}

		~M2TSMux() {
			gf_m2ts_mux_del(muxer);
		}

		void addStream(std::unique_ptr<GF_ESInterface> ifce, u32 PID, Bool isPCR) {
			auto stream = gf_m2ts_program_stream_add(program, ifce.get(), PID, isPCR, GF_FALSE);
			if (ifce->stream_type == GF_STREAM_VISUAL) stream->start_pes_at_rap = GF_TRUE;
			ifces.push_back(std::move(ifce));
		}

		/*return nullptr or a 188 byte packet*/
		const char* process(u32 &status, u32 &usec_till_next) {
			return gf_m2ts_mux_process(muxer, &status, &usec_till_next);
		}

	private:
		GF_M2TS_Mux *muxer = nullptr;
		GF_M2TS_Mux_Program *program = nullptr;
		std::vector<std::unique_ptr<GF_ESInterface>> ifces;
};
#endif /*GPAC_DISABLE_MPEG2TS_MUX*/

struct Mutex : public Init {
		Mutex(const std::string &name) {
			mx = gf_mx_new(name.c_str());
			if (!mx)
				throw std::runtime_error(format("[GPACPP Mutex] Could not create mutex (name=\"%s\")", name));
		}
		~Mutex() {
			gf_mx_del(mx);
		}
		void lock() {
			int ret = gf_mx_p(mx);
			if (ret == 0)
				throw std::runtime_error("[GPACPP Mutex] Unexpected error why trying to lock.");
		}
		void unlock() {
			gf_mx_v(mx);
		}

	private:
		GF_Mutex *mx;
};

struct MutexLock {
		MutexLock(Mutex *mx) : mx(mx) {
			mx->lock();
		}
		~MutexLock() {
			mx->unlock();
		}

	private:
		Mutex *mx;
};

}
