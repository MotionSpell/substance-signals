#include "gpac_mux_mp4.hpp"
#include "lib_utils/tools.hpp"
#include "lib_utils/time.hpp"
#include "lib_gpacpp/gpacpp.hpp"
#include "lib_ffpp/ffpp.hpp"
#include <sstream>

extern "C" {
#include <gpac/base_coding.h>
#include <gpac/constants.h>
#include <gpac/internal/media_dev.h>
}

//#define AVC_INBAND_CONFIG
#define TIMESCALE_MUL 100 /*offers a tolerance on VFR or faulty streams*/

namespace Modules {

namespace {

uint64_t fileSize(const std::string &fn) {
	FILE *file = gf_fopen(fn.c_str(), "rb");
	if (!file) {
		return 0;
	}
	gf_fseek(file, 0, SEEK_END);
	auto const size = gf_ftell(file);
	gf_fseek(file, 0, SEEK_SET);
	gf_fclose(file);
	return size;
}

void getBsContent(GF_ISOFile *iso, char *&output, u32 &size, bool newBs) {
	GF_BitStream *bs = NULL;
	GF_Err e = gf_isom_get_bs(iso, &bs);
	if (e)
		throw std::runtime_error(format("gf_isom_get_bs: %s", gf_error_to_string(e)));
	gf_bs_get_content(bs, &output, &size);
	if (newBs) {
		auto bsNew = gf_bs_new(nullptr, 0, GF_BITSTREAM_WRITE);
		memcpy(bs, bsNew, 2*sizeof(void*)); //HACK: GPAC GF_BitStream.original nee sto be non-NULL
		memset(bsNew,  0, 2*sizeof(void*));
		gf_bs_del(bsNew);
	}
}

static GF_Err avc_import_ffextradata(const u8 *extradata, const u64 extradataSize, GF_AVCConfig *dstcfg) {
	u8 nalSize;
	auto avc = uptr(new AVCState);
	GF_BitStream *bs = nullptr;
	if (!extradata || !extradataSize) {
		Log::msg(Warning, "No initial SPS/PPS provided.");
		return GF_OK;
	}
	bs = gf_bs_new((const char*)extradata, extradataSize, GF_BITSTREAM_READ);
	if (!bs) {
		return GF_BAD_PARAM;
	}
	if (gf_bs_read_u32(bs) != 0x00000001) {
		gf_bs_del(bs);
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	//SPS
	u64 nalStart = 4;
	{
		s32 idx = 0;
		char *buffer = nullptr;
parse_sps:
		nalSize = gf_media_nalu_next_start_code_bs(bs);
		if (nalStart + nalSize > extradataSize) {
			gf_bs_del(bs);
			return GF_BAD_PARAM;
		}
		buffer = (char*)gf_malloc(nalSize);
		gf_bs_read_data(bs, buffer, nalSize);
		gf_bs_seek(bs, nalStart);
		auto const type = gf_bs_read_u8(bs) & 0x1F;
		if (type == GF_AVC_NALU_ACCESS_UNIT) {
			nalStart += nalSize + 4;
			gf_bs_seek(bs, nalStart);
			gf_free(buffer);
			goto parse_sps;
		}
		if (type != GF_AVC_NALU_SEQ_PARAM) {
			gf_bs_del(bs);
			gf_free(buffer);
			return GF_BAD_PARAM;
		}

		idx = gf_media_avc_read_sps(buffer, nalSize, avc.get(), 0, nullptr);
		if (idx < 0) {
			gf_bs_del(bs);
			gf_free(buffer);
			return GF_BAD_PARAM;
		}

		dstcfg->configurationVersion = 1;
		dstcfg->profile_compatibility = avc->sps[idx].prof_compat;
		dstcfg->AVCProfileIndication = avc->sps[idx].profile_idc;
		dstcfg->AVCLevelIndication = avc->sps[idx].level_idc;
		dstcfg->chroma_format = avc->sps[idx].chroma_format;
		dstcfg->luma_bit_depth = 8 + avc->sps[idx].luma_bit_depth_m8;
		dstcfg->chroma_bit_depth = 8 + avc->sps[idx].chroma_bit_depth_m8;

		{
			GF_AVCConfigSlot *slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
			slc->size = nalSize;
			slc->id = idx;
			slc->data = buffer;
			gf_list_add(dstcfg->sequenceParameterSets, slc);
		}
	}

	//PPS
	{
		s32 idx = 0;
		char *buffer = nullptr;
		nalStart += 4 + nalSize;
		gf_bs_seek(bs, nalStart);
		nalSize = gf_media_nalu_next_start_code_bs(bs);
		if (nalStart + nalSize > extradataSize) {
			gf_bs_del(bs);
			return GF_BAD_PARAM;
		}
		buffer = (char*)gf_malloc(nalSize);
		gf_bs_read_data(bs, buffer, nalSize);
		gf_bs_seek(bs, nalStart);
		if ((gf_bs_read_u8(bs) & 0x1F) != GF_AVC_NALU_PIC_PARAM) {
			gf_bs_del(bs);
			gf_free(buffer);
			return GF_BAD_PARAM;
		}

		idx = gf_media_avc_read_pps(buffer, nalSize, avc.get());
		if (idx < 0) {
			gf_bs_del(bs);
			gf_free(buffer);
			return GF_BAD_PARAM;
		}

		{
			GF_AVCConfigSlot *slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
			slc->size = nalSize;
			slc->id = idx;
			slc->data = buffer;
			gf_list_add(dstcfg->pictureParameterSets, slc);
		}
	}

	gf_bs_del(bs);
	return GF_OK;
}

/**
* A function which takes FFmpeg H265 extradata (SPS/PPS) and bring them ready to be pushed to the MP4 muxer.
* @param extradata
* @param extradata_size
* @param dstcfg
* @returns GF_OK is the extradata was parsed and is valid, other values otherwise.
*/
static GF_Err hevc_import_ffextradata(const u8 *extradata, const u64 extradata_size, GF_HEVCConfig *dstCfg) {
	auto hevc = uptr(new HEVCState);
	GF_HEVCParamArray *vpss = nullptr, *spss = nullptr, *ppss = nullptr;
	GF_BitStream *bs = nullptr;
	char *buffer = nullptr;
	u32 bufferSize = 0;
	if (!extradata || (extradata_size < sizeof(u32)))
		return GF_BAD_PARAM;
	bs = gf_bs_new((const char*)extradata, extradata_size, GF_BITSTREAM_READ);
	if (!bs)
		return GF_BAD_PARAM;

	memset(hevc.get(), 0, sizeof(HEVCState));
	hevc->sps_active_idx = -1;

	while (gf_bs_available(bs)) {
		s32 idx = 0;
		GF_AVCConfigSlot *slc = nullptr;
		u8 NALUnitType, temporalId, layerId;
		u64 NALStart = 0;
		u32 NALSize = 0;
		const u32 startCode = gf_bs_read_u24(bs);
		if (!(startCode == 0x000001) && !(!startCode && gf_bs_read_u8(bs) == 1)) {
			gf_bs_del(bs);
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		NALStart = gf_bs_get_position(bs);
		NALSize = gf_media_nalu_next_start_code_bs(bs);
		if (NALStart + NALSize > extradata_size) {
			gf_bs_del(bs);
			return GF_BAD_PARAM;
		}

		if (NALSize > bufferSize) {
			buffer = (char*)gf_realloc(buffer, NALSize);
			bufferSize = NALSize;
		}
		gf_bs_read_data(bs, buffer, NALSize);
		gf_bs_seek(bs, NALStart);

		gf_media_hevc_parse_nalu(buffer, NALSize, hevc.get(), &NALUnitType, &temporalId, &layerId);
		if (layerId) {
			gf_bs_del(bs);
			gf_free(buffer);
			return GF_BAD_PARAM;
		}

		switch (NALUnitType) {
		case GF_HEVC_NALU_VID_PARAM:
			idx = gf_media_hevc_read_vps(buffer, NALSize, hevc.get());
			if (idx < 0) {
				gf_bs_del(bs);
				gf_free(buffer);
				return GF_BAD_PARAM;
			}

			assert(hevc->vps[idx].state == 1); //we don't expect multiple VPS
			if (hevc->vps[idx].state == 1) {
				hevc->vps[idx].state = 2;
				hevc->vps[idx].crc = gf_crc_32(buffer, NALSize);

				dstCfg->avgFrameRate = hevc->vps[idx].rates[0].avg_pic_rate;
				dstCfg->constantFrameRate = hevc->vps[idx].rates[0].constand_pic_rate_idc;
				dstCfg->numTemporalLayers = hevc->vps[idx].max_sub_layers;
				dstCfg->temporalIdNested = hevc->vps[idx].temporal_id_nesting;

				if (!vpss) {
					GF_SAFEALLOC(vpss, GF_HEVCParamArray);
					vpss->nalus = gf_list_new();
					gf_list_add(dstCfg->param_array, vpss);
					vpss->array_completeness = 1;
					vpss->type = GF_HEVC_NALU_VID_PARAM;
				}

				slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
				slc->size = NALSize;
				slc->id = idx;
				slc->data = (char*)gf_malloc(sizeof(char)*slc->size);
				memcpy(slc->data, buffer, sizeof(char)*slc->size);

				gf_list_add(vpss->nalus, slc);
			}
			break;
		case GF_HEVC_NALU_SEQ_PARAM:
			idx = gf_media_hevc_read_sps(buffer, NALSize, hevc.get());
			if (idx < 0) {
				gf_bs_del(bs);
				gf_free(buffer);
				return GF_BAD_PARAM;
			}

			assert(!(hevc->sps[idx].state & AVC_SPS_DECLARED)); //we don't expect multiple SPS
			if ((hevc->sps[idx].state & AVC_SPS_PARSED) && !(hevc->sps[idx].state & AVC_SPS_DECLARED)) {
				hevc->sps[idx].state |= AVC_SPS_DECLARED;
				hevc->sps[idx].crc = gf_crc_32(buffer, NALSize);
			}

			dstCfg->configurationVersion = 1;
			dstCfg->profile_space = hevc->sps[idx].ptl.profile_space;
			dstCfg->tier_flag = hevc->sps[idx].ptl.tier_flag;
			dstCfg->profile_idc = hevc->sps[idx].ptl.profile_idc;
			dstCfg->general_profile_compatibility_flags = hevc->sps[idx].ptl.profile_compatibility_flag;
			dstCfg->progressive_source_flag = hevc->sps[idx].ptl.general_progressive_source_flag;
			dstCfg->interlaced_source_flag = hevc->sps[idx].ptl.general_interlaced_source_flag;
			dstCfg->non_packed_constraint_flag = hevc->sps[idx].ptl.general_non_packed_constraint_flag;
			dstCfg->frame_only_constraint_flag = hevc->sps[idx].ptl.general_frame_only_constraint_flag;

			dstCfg->constraint_indicator_flags = hevc->sps[idx].ptl.general_reserved_44bits;
			dstCfg->level_idc = hevc->sps[idx].ptl.level_idc;

			dstCfg->chromaFormat = hevc->sps[idx].chroma_format_idc;
			dstCfg->luma_bit_depth = hevc->sps[idx].bit_depth_luma;
			dstCfg->chroma_bit_depth = hevc->sps[idx].bit_depth_chroma;

			if (!spss) {
				GF_SAFEALLOC(spss, GF_HEVCParamArray);
				spss->nalus = gf_list_new();
				gf_list_add(dstCfg->param_array, spss);
				spss->array_completeness = 1;
				spss->type = GF_HEVC_NALU_SEQ_PARAM;
			}

			slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
			slc->size = NALSize;
			slc->id = idx;
			slc->data = (char*)gf_malloc(sizeof(char)*slc->size);
			memcpy(slc->data, buffer, sizeof(char)*slc->size);
			gf_list_add(spss->nalus, slc);
			break;
		case GF_HEVC_NALU_PIC_PARAM:
			idx = gf_media_hevc_read_pps(buffer, NALSize, hevc.get());
			if (idx < 0) {
				gf_bs_del(bs);
				gf_free(buffer);
				return GF_BAD_PARAM;
			}

			assert(hevc->pps[idx].state == 1); //we don't expect multiple PPS
			if (hevc->pps[idx].state == 1) {
				hevc->pps[idx].state = 2;
				hevc->pps[idx].crc = gf_crc_32(buffer, NALSize);

				if (!ppss) {
					GF_SAFEALLOC(ppss, GF_HEVCParamArray);
					ppss->nalus = gf_list_new();
					gf_list_add(dstCfg->param_array, ppss);
					ppss->array_completeness = 1;
					ppss->type = GF_HEVC_NALU_PIC_PARAM;
				}

				slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
				slc->size = NALSize;
				slc->id = idx;
				slc->data = (char*)gf_malloc(sizeof(char)*slc->size);
				memcpy(slc->data, buffer, sizeof(char)*slc->size);

				gf_list_add(ppss->nalus, slc);
			}
			break;
		default:
			break;
		}

		if (gf_bs_seek(bs, NALStart + NALSize)) {
			assert(NALStart + NALSize <= gf_bs_get_size(bs));
			break;
		}
	}

	gf_bs_del(bs);
	gf_free(buffer);

	return GF_OK;
}

void fillVideoSampleData(const u8 *bufPtr, u32 bufLen, GF_ISOSample &sample) {
	u32 scSize = 0, NALUSize = 0;
	GF_BitStream *out_bs = gf_bs_new(nullptr, 2 * bufLen, GF_BITSTREAM_WRITE);
	NALUSize = gf_media_nalu_next_start_code(bufPtr, bufLen, &scSize);
	if (NALUSize != 0) {
		gf_bs_write_u32(out_bs, NALUSize);
		gf_bs_write_data(out_bs, (const char*)bufPtr, NALUSize);
	}
	if (scSize) {
		bufPtr += (NALUSize + scSize);
		bufLen -= (NALUSize + scSize);
	}

	while (bufLen) {
		NALUSize = gf_media_nalu_next_start_code(bufPtr, bufLen, &scSize);
		if (NALUSize != 0) {
			gf_bs_write_u32(out_bs, NALUSize);
			gf_bs_write_data(out_bs, (const char*)bufPtr, NALUSize);
		}

		bufPtr += NALUSize;

		if (!scSize || (bufLen < NALUSize + scSize))
			break;
		bufLen -= NALUSize + scSize;
		bufPtr += scSize;
	}
	gf_bs_get_content(out_bs, &sample.data, &sample.dataLength);
	gf_bs_del(out_bs);
}
}

namespace Mux {

GPACMuxMP4::GPACMuxMP4(const std::string &baseName, uint64_t segmentDurationInMs, SegmentPolicy segmentPolicy, FragmentPolicy fragmentPolicy, CompatibilityFlag compatFlags)
: compatFlags(compatFlags), fragmentPolicy(fragmentPolicy), segmentPolicy(segmentPolicy), segmentDurationIn180k(timescaleToClock(segmentDurationInMs, 1000)) {
	if ((segmentDurationInMs == 0) ^ (segmentPolicy == NoSegment || segmentPolicy == SingleSegment))
		throw error(format("Inconsistent parameters: segment duration is %sms but no segment.", segmentDurationInMs));
	if ((segmentDurationInMs == 0) && (fragmentPolicy == Mux::GPACMuxMP4::OneFragmentPerSegment))
		throw error("Inconsistent parameters: segment duration is 0 ms but requested one fragment by segment.");
	if ((segmentPolicy == SingleSegment || segmentPolicy == FragmentedSegment) && (fragmentPolicy == NoFragment))
		throw error("Inconsistent parameters: segmented policies require fragmentation to be enabled.");
	if ((compatFlags & SmoothStreaming) && (segmentPolicy != IndependentSegment))
		throw error("Inconsistent parameters: SmoothStreaming compatibility requires IndependentSegment policy.");
	if ((compatFlags & FlushFragMemory) && ((!baseName.empty()) || (segmentPolicy != FragmentedSegment)))
		throw error("Inconsistent parameters: FlushFragMemory requires an empty segment name and FragmentedSegment policy.");

	if (baseName.empty()) {
		log(Info, "Working in memory mode.");
	} else {
		if (segmentPolicy > NoSegment) {
			segmentName = format("%s-init.mp4", baseName);
		} else {
			segmentName = format("%s.mp4", baseName);
		}
		
		log(Info, "Working in file mode: %s.", segmentName);
	}

	isoInit = gf_isom_open(segmentName.empty() ? nullptr : segmentName.c_str(), GF_ISOM_OPEN_WRITE, nullptr);
	if (!isoInit)
		throw error(format("Cannot open isoInit file %s"));
	isoCur = isoInit;

	GF_Err e = gf_isom_set_storage_mode(isoCur, GF_ISOM_STORE_INTERLEAVED);
	if (e != GF_OK)
		throw error(format("Cannot make iso file %s interleaved", baseName));

	if (compatFlags & FlushFragMemory) {
		output = addOutputDynAlloc<OutputDataDefault<DataRawGPAC>>(100 * ALLOC_NUM_BLOCKS_DEFAULT); //TODO: retrieve framerate, and multiply the allocator size
	} else {
		output = addOutput<OutputDataDefault<DataRawGPAC>>();
	}
}

void GPACMuxMP4::flush() {
	if (compatFlags & ExactInputDur) {
		inputs[0]->push(lastData);
		process();
		lastData = nullptr;
	}
	closeSegment(true);

	if (segmentPolicy == IndependentSegment) {
		if (gf_isom_get_filename(isoInit)) {
			const std::string fn = gf_isom_get_filename(isoInit);
			gf_isom_delete(isoInit);
			gf_delete_file(fn.c_str());
		} else {
			gf_isom_delete(isoInit);
		}
	} else {
		GF_Err e = gf_isom_close(isoCur);
		if (e != GF_OK && e != GF_ISOM_INVALID_FILE)
			throw error(format("gf_isom_close: %s", gf_error_to_string(e)));
	}
}

void GPACMuxMP4::startSegment() {
	if (segmentPolicy > SingleSegment) {
		if (gf_isom_get_filename(isoInit)) {
			std::stringstream ss;
			std::string fn = gf_isom_get_filename(isoInit);
			ss << fn.substr(0, fn.find("-init")) << "-" << segmentNum;
			if (segmentPolicy == FragmentedSegment) ss << ".m4s";
			else ss << ".mp4";
			segmentName = ss.str();
		}

		if (segmentPolicy == FragmentedSegment) {
			GF_Err e = gf_isom_start_segment(isoCur, segmentName.empty() ? nullptr : segmentName.c_str(), GF_TRUE);
			if (e != GF_OK)
				throw error(format("Impossible to start segment %s (%s): %s", segmentNum, segmentName, gf_error_to_string(e)));
		} else if (segmentPolicy == IndependentSegment) {
			isoCur = gf_isom_open(segmentName.empty() ? nullptr : segmentName.c_str(), GF_ISOM_OPEN_WRITE, nullptr);
			if (!isoCur)
				throw error(format("Cannot open isoCur file %s"));
			declareStream(inputs[0]->getMetadata());
			startSegmentPostAction();
			setupFragments();
			gf_isom_set_next_moof_number(isoCur, (u32)nextFragmentNum);
		} else
			throw error("Unknown segment policy (2)");
	}
}

void GPACMuxMP4::closeSegment(bool isLastSeg) {
	if (curFragmentDurInTs) {
		closeFragment();
	}

	if (!isLastSeg && segmentPolicy <= SingleSegment) {
		return;
	} else {
		if (segmentPolicy == FragmentedSegment) {
			GF_Err e = gf_isom_close_segment(isoCur, 0, 0, 0, 0, 0, GF_FALSE, (Bool)isLastSeg, (Bool)(gf_isom_get_filename(isoInit) != nullptr),
			                                 (compatFlags & Browsers) ? 0 : GF_4CC('e', 'o', 'd', 's'), nullptr, nullptr, &lastSegmentSize);
			if (e != GF_OK) {
				if (DTS == 0) {
					return;
				} else
					throw error(format("gf_isom_close_segment: %s", gf_error_to_string(e)));
			}
		}

		sendOutput(true);
		log(Debug, "Segment %s completed (size %s) (startsWithSAP=%s)", segmentName.empty() ? "[in memory]" : segmentName, lastSegmentSize, segmentStartsWithRAP);

		curSegmentDurInTs = 0;
	}
}

void GPACMuxMP4::startFragment(uint64_t DTS, uint64_t PTS) {
	if (fragmentPolicy > NoFragment) {
		curFragmentDurInTs = 0;

		GF_Err e = gf_isom_start_fragment(isoCur, GF_TRUE);
		if (e != GF_OK)
			throw error(format("Impossible to create the moof starting the fragment: %s", gf_error_to_string(e)));

		if (segmentPolicy >= IndependentSegment) {
			if (compatFlags & SmoothStreaming) {
				e = gf_isom_set_fragment_option(isoCur, trackId, GF_ISOM_TFHD_FORCE_MOOF_BASE_OFFSET, 1);
				if (e != GF_OK)
					throw error(format("Cannot force the use of moof base offsets: %s", gf_error_to_string(e)));
			} else {
				e = gf_isom_set_traf_base_media_decode_time(isoCur, trackId, DTS);
				if (e != GF_OK)
					throw error(format("Impossible to create TFDT %s: %s", DTS, gf_error_to_string(e)));
			}

			if (!(compatFlags & Browsers)) {
				e = gf_isom_set_fragment_reference_time(isoCur, trackId, UTC2NTP(firstDataAbsTimeInMs) + PTS, PTS);
				if (e != GF_OK)
					throw error(format("Impossible to create UTC marquer: %s", gf_error_to_string(e)));
			}
		}
	}
}

void GPACMuxMP4::closeFragment() {
	if (fragmentPolicy > NoFragment) {
		if (!curFragmentDurInTs) {
			log((compatFlags & Browsers) ? Error : Warning, "Writing an empty fragment. Some players may stop playing here.");
		}
		if (compatFlags & SmoothStreaming) {
			if (mediaTs == 0) {
				log(Warning, "Media timescale is 0. Fragment cannot be closed.");
				return;
			}

			auto const curFragmentStartInTs = DTS - curFragmentDurInTs;
			auto const absTimeInTs = convertToTimescale(firstDataAbsTimeInMs, 1000, mediaTs) + curFragmentStartInTs;
			auto const deltaRealTimeInMs = 1000 * (double)(getUTC() - Fraction(absTimeInTs, mediaTs));
			log(deltaRealTimeInMs < 0 || deltaRealTimeInMs > curFragmentStartInTs || curFragmentDurInTs != clockToTimescale(segmentDurationIn180k, mediaTs) ? Warning : Debug,
				"Closing MSS fragment with absolute time %s %s UTC and duration %s (timescale %s, time=%s, deltaRT=%s)",
				getDay(), getTimeFromUTC(), curFragmentDurInTs, mediaTs, absTimeInTs, deltaRealTimeInMs);
			GF_Err e = gf_isom_set_traf_mss_timeext(isoCur, trackId, absTimeInTs, curFragmentDurInTs);
			if (e != GF_OK)
				throw error(format("Impossible to create UTC marker: %s", gf_error_to_string(e)));
		}
		if ((segmentPolicy == FragmentedSegment) || (segmentPolicy == SingleSegment)) {
			gf_isom_flush_fragments(isoCur, GF_FALSE); //writes a 'styp'

			if (compatFlags & FlushFragMemory) {
				sendOutput(false);
			}
		}

		curFragmentDurInTs = 0;
	}
}

void GPACMuxMP4::setupFragments() {
	if (fragmentPolicy > NoFragment) {
		GF_Err e = gf_isom_setup_track_fragment(isoCur, trackId, 1, compatFlags & SmoothStreaming ? 0 : (u32)defaultSampleIncInTs, 0, 0, 0, 0);
		if (e != GF_OK)
			throw error(format("Cannot setup track as fragmented: %s", gf_error_to_string(e)));

		int mode = 1;
		if (segmentPolicy == NoSegment || segmentPolicy == IndependentSegment) mode = 0;
		else if (segmentPolicy == SingleSegment) mode = 2;
		e = gf_isom_finalize_for_fragment(isoCur, mode); //writes moov
		if (e != GF_OK)
			throw error(format("Cannot prepare track for movie fragmentation: %s", gf_error_to_string(e)));

		if (segmentPolicy == FragmentedSegment) {
			sendOutput(true); //init
		}
	}
}

void GPACMuxMP4::declareStreamAudio(const std::shared_ptr<const MetadataPktLibavAudio> &metadata) {
	GF_Err e;
	u32 di=0, trackNum=0;
	GF_M4ADecSpecInfo acfg;

	GF_ESD *esd = gf_odf_desc_esd_new(2);
	if (!esd)
		throw error(format("Cannot create GF_ESD"));

	const uint8_t *extradata;
	size_t extradataSize;
	metadata->getExtradata(extradata, extradataSize);
	esd->decoderConfig = (GF_DecoderConfig *)gf_odf_desc_new(GF_ODF_DCD_TAG);
	esd->slConfig = (GF_SLConfig *)gf_odf_desc_new(GF_ODF_SLC_TAG);
	esd->decoderConfig->streamType = GF_STREAM_AUDIO;
	sampleRate = metadata->getSampleRate();
	if (metadata->getCodecName() == "aac") { //TODO: find an automatic table, we only know about MPEG1 Layer 2 and AAC-LC
		codec4CC = "AACL";
		esd->decoderConfig->objectTypeIndication = GPAC_OTI_AUDIO_AAC_MPEG4;

		esd->decoderConfig->bufferSizeDB = 20;
		esd->slConfig->timestampResolution = sampleRate;
		esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *)gf_odf_desc_new(GF_ODF_DSI_TAG);
		esd->ESID = 1;

		esd->decoderConfig->decoderSpecificInfo->dataLength = (u32)extradataSize;
		esd->decoderConfig->decoderSpecificInfo->data = (char*)gf_malloc(extradataSize);
		memcpy(esd->decoderConfig->decoderSpecificInfo->data, extradata, extradataSize);

		memset(&acfg, 0, sizeof(GF_M4ADecSpecInfo));
		acfg.base_object_type = GF_M4A_AAC_LC;
		acfg.base_sr = sampleRate;
		acfg.nb_chan = metadata->getNumChannels();
		acfg.sbr_object_type = 0;
		acfg.audioPL = gf_m4a_get_profile(&acfg);

		/*e = gf_m4a_write_config(&acfg, &esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength);
		assert(e == GF_OK);*/
	} else {
		if (metadata->getCodecName() != "mp2") {
			log(Warning, "Unlisted codec, setting GPAC_OTI_AUDIO_MPEG1 descriptor.");
		}
		esd->decoderConfig->objectTypeIndication = GPAC_OTI_AUDIO_MPEG1;
		esd->decoderConfig->bufferSizeDB = 20;
		esd->slConfig->timestampResolution = sampleRate;
		esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *)gf_odf_desc_new(GF_ODF_DSI_TAG);
		esd->ESID = 1;

		memset(&acfg, 0, sizeof(GF_M4ADecSpecInfo));
		acfg.base_object_type = GF_M4A_LAYER2;
		acfg.base_sr = sampleRate;
		acfg.nb_chan = metadata->getNumChannels();
		acfg.sbr_object_type = 0;
		acfg.audioPL = gf_m4a_get_profile(&acfg);

		e = gf_m4a_write_config(&acfg, &esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength);
		assert(e == GF_OK);
	}

	mediaTs = sampleRate;
	trackNum = gf_isom_new_track(isoCur, esd->ESID, GF_ISOM_MEDIA_AUDIO, mediaTs);
	log(Debug, "TimeScale: %s", mediaTs);
	if (!trackNum)
		throw error(format("Cannot create new track"));
	trackId = gf_isom_get_track_id(isoCur, trackNum);
	defaultSampleIncInTs = metadata->getFrameSize();

	e = gf_isom_set_track_enabled(isoCur, trackNum, GF_TRUE);
	if (e != GF_OK)
		throw error(format("gf_isom_set_track_enabled: %s", gf_error_to_string(e)));

	e = gf_isom_new_mpeg4_description(isoCur, trackNum, esd, nullptr, nullptr, &di);
	if (e != GF_OK)
		throw error(format("gf_isom_new_mpeg4_description: %s", gf_error_to_string(e)));

	gf_odf_desc_del((GF_Descriptor *)esd);
	esd = nullptr;

	auto const bitsPerSample = metadata->getBitsPerSample() >= 16 ? 16 : metadata->getBitsPerSample();
	e = gf_isom_set_audio_info(isoCur, trackNum, di, sampleRate, metadata->getNumChannels(), bitsPerSample);
	if (e != GF_OK)
		throw error(format("gf_isom_set_audio_info: %s", gf_error_to_string(e)));

	e = gf_isom_set_pl_indication(isoCur, GF_ISOM_PL_AUDIO, acfg.audioPL);
	if (e != GF_OK)
		throw error(format("Container format import failed: %s", gf_error_to_string(e)));

	if (!(compatFlags & SegmentAtAny)) {
		log(Info, "Audio detected: assuming all segments are RAPs.");
		compatFlags = compatFlags | SegmentAtAny;
	}
}

void GPACMuxMP4::declareStreamSubtitle(const std::shared_ptr<const MetadataPktLibavSubtitle> &metadata) {
	mediaTs = 10 * TIMESCALE_MUL;
	assert(((10 * TIMESCALE_MUL) % 1000) == 0); /*ms accuracy mandatory*/
	u32 trackNum = gf_isom_new_track(isoCur, 0, GF_ISOM_MEDIA_TEXT, mediaTs);
	if (!trackNum)
		throw error(format("Cannot create new track"));
	trackId = gf_isom_get_track_id(isoCur, trackNum);

	defaultSampleIncInTs = clockToTimescale(segmentDurationIn180k, mediaTs);
	if (segmentDurationIn180k != timescaleToClock(defaultSampleIncInTs, mediaTs))
		throw error(format("Rounding error when computing default sample duration for subtitles (%s vs %s, timescale=%s)", segmentDurationIn180k, timescaleToClock(defaultSampleIncInTs, mediaTs), mediaTs));

	GF_Err e = gf_isom_set_track_enabled(isoCur, trackNum, GF_TRUE);
	if (e != GF_OK)
		throw error(format("gf_isom_set_track_enabled: %s", gf_error_to_string(e)));

	u32 di;
	e = gf_isom_new_xml_subtitle_description(isoCur, trackNum, "http://www.w3.org/ns/ttml", NULL, NULL, &di);
	if (e != GF_OK)
		throw error(format("gf_isom_new_xml_subtitle_description: %s", gf_error_to_string(e)));

	codec4CC = "TTML";
	if (!(compatFlags & SegmentAtAny)) {
		log(Info, "Subtitles detected: assuming all segments are RAPs.");
		compatFlags = compatFlags | SegmentAtAny;
	}
}

void GPACMuxMP4::declareStreamVideo(const std::shared_ptr<const MetadataPktLibavVideo> &metadata) {
	mediaTs = (uint32_t)(metadata->getTimeScale().num * TIMESCALE_MUL);
	u32 trackNum = gf_isom_new_track(isoCur, 0, GF_ISOM_MEDIA_VISUAL, mediaTs);
	if (!trackNum)
		throw error(format("Cannot create new track"));
	trackId = gf_isom_get_track_id(isoCur, trackNum);
	defaultSampleIncInTs = metadata->getTimeScale().den * TIMESCALE_MUL;
	resolution[0] = metadata->getResolution().width;
	resolution[1] = metadata->getResolution().height;

	GF_Err e = gf_isom_set_track_enabled(isoCur, trackNum, GF_TRUE);
	if (e != GF_OK)
		throw error(format("Cannot enable track: %s", gf_error_to_string(e)));

	const uint8_t *extradata;
	size_t extradataSize;
	metadata->getExtradata(extradata, extradataSize);

	u32 di = 0;
	if (metadata->getAVCodecContext()->codec_id == AV_CODEC_ID_H264) {
		codec4CC = "H264";
		GF_AVCConfig *avccfg = gf_odf_avc_cfg_new();
		if (!avccfg)
			throw error(format("Container format import failed (AVC)"));

		e = avc_import_ffextradata(extradata, extradataSize, avccfg);
		if (e == GF_OK) {
			e = gf_isom_avc_config_new(isoCur, trackNum, avccfg, nullptr, nullptr, &di);
			if (e != GF_OK)
				throw error(format("Cannot create AVC config: %s", gf_error_to_string(e)));
		}
		gf_odf_avc_cfg_del(avccfg);
	} else if (metadata->getAVCodecContext()->codec_id == AV_CODEC_ID_H265) {
		codec4CC = "H265";
		GF_HEVCConfig *hevccfg = gf_odf_hevc_cfg_new();
		if (!hevccfg)
			throw error(format("Container format import failed (HEVC)"));

		e = hevc_import_ffextradata(extradata, extradataSize, hevccfg);
		if (e == GF_OK) {
			e = gf_isom_hevc_config_new(isoCur, trackNum, hevccfg, nullptr, nullptr, &di);
			if (e != GF_OK)
				throw error(format("Cannot create AVC config: %s", gf_error_to_string(e)));
		}
		gf_odf_hevc_cfg_del(hevccfg);
	} else {
		throw error(format("Unknown codec"));
	}
	if (e) {
		if (e == GF_NON_COMPLIANT_BITSTREAM) {
			log(Debug, "non Annex B: assume this is AVCC");
			isAnnexB = false;

			GF_ESD *esd = (GF_ESD *)gf_odf_desc_esd_new(0);
			esd->ESID = 1; /*FIXME: only one track: set trackID?*/
			esd->decoderConfig->streamType = GF_STREAM_VISUAL;
			esd->decoderConfig->avgBitrate = esd->decoderConfig->maxBitrate = 0;
			esd->decoderConfig->objectTypeIndication = metadata->getAVCodecContext()->codec_id == AV_CODEC_ID_H264 ? GPAC_OTI_VIDEO_AVC : GPAC_OTI_VIDEO_HEVC;
			esd->decoderConfig->decoderSpecificInfo->dataLength = (u32)extradataSize;
			esd->decoderConfig->decoderSpecificInfo->data = (char*)gf_malloc(extradataSize);
			memcpy(esd->decoderConfig->decoderSpecificInfo->data, extradata, extradataSize);
			esd->slConfig->predefined = SLPredef_MP4;

			e = gf_isom_new_mpeg4_description(isoCur, trackNum, esd, nullptr, nullptr, &di);
			if (e != GF_OK)
				throw error(format("Cannot create MPEG-4 config: %s", gf_error_to_string(e)));
			gf_odf_desc_del((GF_Descriptor*)esd);
		} else {
			throw error(format("Container format import failed"));
		}
	}

	auto const res = metadata->getResolution();
	resolution[0] = res.width;
	resolution[1] = res.height;
	gf_isom_set_visual_info(isoCur, gf_isom_get_track_by_id(isoCur, trackId), di, res.width, res.height);
	gf_isom_set_sync_table(isoCur, trackNum);

#ifdef AVC_INBAND_CONFIG
	//inband SPS/PPS
	if (m_useSegments) {
		e = gf_isom_avc_set_inband_config(isoCur, trackNum, di);
		if (e != GF_OK)
			throw error(format("Cannot set inband PPS/SPS for AVC track: %s", gf_error_to_string(e)));
	}
#endif
}

void GPACMuxMP4::declareStream(const std::shared_ptr<const IMetadata> &metadata) {
	if (auto video = std::dynamic_pointer_cast<const MetadataPktLibavVideo>(metadata)) {
		declareStreamVideo(video);
	} else if (auto audio = std::dynamic_pointer_cast<const MetadataPktLibavAudio>(metadata)) {
		declareStreamAudio(audio);
	} else if (auto subs = std::dynamic_pointer_cast<const MetadataPktLibavSubtitle>(metadata)) {
		declareStreamSubtitle(subs);
	} else
		throw error(format("Stream creation failed: unknown type."));
}

void GPACMuxMP4::handleInitialTimeOffset() {
	if (initTimeIn180k) { /*first timestamp is not zero*/
		log(Info, "Initial offset: %ss (4CC=%s, \"%s\", timescale=%s/%s)", initTimeIn180k / (double)Clock::Rate, codec4CC, segmentName, mediaTs, gf_isom_get_timescale(isoCur));
		if (compatFlags & NoEditLists) {
			firstDataAbsTimeInMs += clockToTimescale(initTimeIn180k, 1000);
		} else {
			auto const edtsInMovieTs = clockToTimescale(initTimeIn180k, gf_isom_get_timescale(isoCur));
			auto const edtsInMediaTs = clockToTimescale(initTimeIn180k, mediaTs);
			if (edtsInMovieTs > 0) {
				gf_isom_append_edit_segment(isoCur, gf_isom_get_track_by_id(isoCur, trackId), edtsInMovieTs, 0, GF_ISOM_EDIT_EMPTY);
				gf_isom_append_edit_segment(isoCur, gf_isom_get_track_by_id(isoCur, trackId), edtsInMovieTs, 0, GF_ISOM_EDIT_NORMAL);
				curSegmentDeltaInTs = edtsInMediaTs;
			} else {
				gf_isom_append_edit_segment(isoCur, gf_isom_get_track_by_id(isoCur, trackId), 0, -edtsInMediaTs, GF_ISOM_EDIT_NORMAL);
			}
		}
	}
}

void GPACMuxMP4::sendOutput(bool EOS) {
	if (segmentPolicy == IndependentSegment) {
		nextFragmentNum = gf_isom_get_next_moof_number(isoCur);
		GF_Err e = gf_isom_write(isoCur);
		if (e)
			throw error(format("gf_isom_write: %s", gf_error_to_string(e)));
	}

	auto out = output->getBuffer(0);
	if (gf_isom_get_filename(isoCur)) {
		lastSegmentSize = fileSize(segmentName);
	} else {
		char *output = nullptr; u32 size = 0;
		getBsContent(isoCur, output, size, (compatFlags & FlushFragMemory) && curFragmentDurInTs);
		if (!size && !EOS) {
			assert((segmentPolicy == FragmentedSegment) && (fragmentPolicy > NoFragment));
			log(Debug, "Empty segment. Ignore.");
			return;
		}
		out->setData((uint8_t*)output, size);
		lastSegmentSize = size;
	}

	StreamType streamType;
	std::string mimeType;
	switch (gf_isom_get_media_type(isoCur, gf_isom_get_track_by_id(isoCur, trackId))) {
	case GF_ISOM_MEDIA_VISUAL: streamType = VIDEO_PKT; mimeType = "video/mp4"; break;
	case GF_ISOM_MEDIA_AUDIO: streamType = AUDIO_PKT; mimeType = "audio/mp4"; break;
	case GF_ISOM_MEDIA_TEXT: streamType = SUBTITLE_PKT; mimeType = "application/mp4"; break;
	default: throw error(format("Segment contains neither audio nor video"));
	}
	Bool isInband =
#ifdef AVC_INBAND_CONFIG
		GF_TRUE;
#else
		GF_FALSE;
#endif
	char codecName[40];
	GF_Err e = gf_media_get_rfc_6381_codec_name(isoCur, gf_isom_get_track_by_id(isoCur, trackId), codecName, isInband, GF_FALSE);
	if (e)
		throw error(format("Could not compute codec name (RFC 6381)"));

	auto const consideredDurationIn180k = (compatFlags & FlushFragMemory) ? timescaleToClock(curFragmentDurInTs, mediaTs) : timescaleToClock(curSegmentDurInTs, mediaTs);
	auto computeContainerLatency = [&]() {
		return fragmentPolicy == OneFragmentPerFrame ? timescaleToClock(defaultSampleIncInTs, mediaTs) : std::min<uint64_t>(consideredDurationIn180k, segmentDurationIn180k);
	};
	auto metadata = std::make_shared<MetadataFile>(segmentName, streamType, mimeType, codecName, consideredDurationIn180k, lastSegmentSize, computeContainerLatency(), segmentStartsWithRAP, EOS);
	switch (gf_isom_get_media_type(isoCur, gf_isom_get_track_by_id(isoCur, trackId))) {
	case GF_ISOM_MEDIA_VISUAL: metadata->resolution[0] = resolution[0]; metadata->resolution[1] = resolution[1]; break;
	case GF_ISOM_MEDIA_AUDIO: metadata->sampleRate = sampleRate; break;
	case GF_ISOM_MEDIA_TEXT: break;
	default: throw error(format("Segment contains neither audio nor video"));
	}

	out->setMetadata(metadata);
	auto const curSegmentStartInTs = DTS - curSegmentDurInTs;
	out->setMediaTime(convertToTimescale(firstDataAbsTimeInMs, 1000, mediaTs) + curSegmentStartInTs, mediaTs);
	output->emit(out);

	if (segmentPolicy == IndependentSegment) {
		gf_isom_delete(isoCur);
		isoCur = nullptr;
	}
}

void GPACMuxMP4::startChunk(gpacpp::IsoSample * const sample) {
	if (curSegmentDurInTs == 0) {
		segmentStartsWithRAP = sample->IsRAP == RAP;
		if (segmentPolicy > SingleSegment) {
			const u64 oneSegDurInTs = clockToTimescale(segmentDurationIn180k, mediaTs);
			if (oneSegDurInTs * (DTS / oneSegDurInTs) == 0) { /*initial delay*/
				curSegmentDeltaInTs = curSegmentDurInTs + curSegmentDeltaInTs - oneSegDurInTs * ((curSegmentDurInTs + curSegmentDeltaInTs) / oneSegDurInTs);
			} else {
				auto const num = (curSegmentDurInTs + curSegmentDeltaInTs) / oneSegDurInTs;
				auto const rem = DTS - (num ? num - 1 : 0) * oneSegDurInTs;
				curSegmentDeltaInTs = DTS - oneSegDurInTs * (rem / oneSegDurInTs);
			}
			if (segmentPolicy == IndependentSegment) {
				sample->DTS = 0;
			}
		}
		if (fragmentPolicy > NoFragment) {
			startFragment(sample->DTS, sample->DTS + sample->CTS_Offset);
		}
	}
}

void GPACMuxMP4::addData(gpacpp::IsoSample const * const sample, int64_t lastDataDurationInTs) {
	if (fragmentPolicy > NoFragment) {
		if (curFragmentDurInTs && (fragmentPolicy == OneFragmentPerRAP) && (sample->IsRAP == RAP)) {
			closeFragment();
			startFragment(sample->DTS, sample->DTS + sample->CTS_Offset);
		}
		if (curSegmentDurInTs && (fragmentPolicy == OneFragmentPerFrame)) {
			startFragment(sample->DTS, sample->DTS + sample->CTS_Offset);
		}

		GF_Err e = gf_isom_fragment_add_sample(isoCur, trackId, sample, 1, (u32)lastDataDurationInTs, 0, 0, GF_FALSE);
		if (e != GF_OK) {
			log(Error, "gf_isom_fragment_add_sample: %s", gf_error_to_string(e));
			return;
		}
		curFragmentDurInTs += lastDataDurationInTs;

		if (fragmentPolicy == OneFragmentPerFrame) {
			closeFragment();
		}
	} else {
		GF_Err e = gf_isom_add_sample(isoCur, trackId, 1, sample);
		if (e != GF_OK) {
			log(Error, "gf_isom_add_sample: %s", gf_error_to_string(e));
			return;
		}
	}

	DTS += lastDataDurationInTs;
	if (segmentPolicy > SingleSegment) {
		curSegmentDurInTs += lastDataDurationInTs;
	}
}

void GPACMuxMP4::closeChunk(bool nextSampleIsRAP) {
	if (segmentPolicy > SingleSegment) {
		if ((!(compatFlags & Browsers) || curFragmentDurInTs > 0 || fragmentPolicy == OneFragmentPerFrame) && /*avoid 0-sized mdat interpreted as EOS in browsers*/
			((curSegmentDurInTs + curSegmentDeltaInTs) * IClock::Rate) >= (mediaTs * segmentDurationIn180k) &&
			((nextSampleIsRAP == RAP) || (compatFlags & SegmentAtAny))) {
			if ((compatFlags & SegConstantDur) && (timescaleToClock(curSegmentDurInTs + curSegmentDeltaInTs, mediaTs) != segmentDurationIn180k) && (curSegmentDurInTs != 0)) {
				if ((DTS / clockToTimescale(segmentDurationIn180k, mediaTs)) <= 1) {
					segmentDurationIn180k = timescaleToClock(curSegmentDurInTs + curSegmentDeltaInTs, mediaTs);
				}
			}
			closeSegment(false);
			segmentNum++;
			startSegment();
		}
	}
}

void GPACMuxMP4::processSample(std::unique_ptr<gpacpp::IsoSample> sample, int64_t lastDataDurationInTs) {
	closeChunk(sample->IsRAP == RAP);
	startChunk(sample.get());
	addData(sample.get(), lastDataDurationInTs);
	closeChunk(false); //close it now if possible, otherwise wait for the next sample to be available
}

std::unique_ptr<gpacpp::IsoSample> GPACMuxMP4::fillSample(Data data_) {
	auto data = safe_cast<const DataAVPacket>(data_);
	auto sample = uptr(new gpacpp::IsoSample);
	u32 bufLen = (u32)data->size();
	const u8 *bufPtr = data->data();

	const u32 mediaType = gf_isom_get_media_type(isoCur, gf_isom_get_track_by_id(isoCur, trackId));
	if (mediaType == GF_ISOM_MEDIA_VISUAL) {
		if (isAnnexB) {
			fillVideoSampleData(bufPtr, bufLen, *sample);
		} else {
			sample->data = (char*)bufPtr;
			sample->dataLength = bufLen;
			sample->setDataOwnership(false);
		}
	} else if (mediaType == GF_ISOM_MEDIA_AUDIO || mediaType == GF_ISOM_MEDIA_TEXT) {
		sample->data = (char*)bufPtr;
		sample->dataLength = bufLen;
		sample->setDataOwnership(false);
	} else
		throw error("Only audio, video or text supported");

	if (segmentPolicy == IndependentSegment) {
		sample->DTS = curSegmentDurInTs + curSegmentDeltaInTs;
	} else {
		sample->DTS = DTS;
	}
	sample->IsRAP = (SAPType)(data->getPacket()->flags & AV_PKT_FLAG_KEY);
	return sample;
}

bool GPACMuxMP4::processInit(Data &data) {
	if (inputs[0]->updateMetadata(data)) {
		auto const &metadata = data->getMetadata();
		declareStream(metadata);

		if (!defaultSampleIncInTs) {
			auto pkt = safe_cast<const DataAVPacket>(data);
			if (pkt && pkt->getPacket()->duration) {
				auto metaPkt = std::dynamic_pointer_cast<const MetadataPktLibav>(metadata);
				defaultSampleIncInTs = convertToTimescale(pkt->getPacket()->duration, metaPkt->getTimeScale().num, metaPkt->getTimeScale().den * mediaTs);
				log(Warning, "Codec defaultSampleIncInTs=0 but first data contains a duration (%s/%s).", defaultSampleIncInTs, mediaTs);
			} else {
				log(Warning, "Computed defaultSampleIncInTs=0, forcing the ExactInputDur flag.");
				compatFlags = compatFlags | ExactInputDur;
			}
		}

		if (!firstDataAbsTimeInMs) {
			firstDataAbsTimeInMs = DataBase::absUTCOffsetInMs;
			initTimeIn180k = data->getMediaTime();
			handleInitialTimeOffset();
		}

		setupFragments();
		if (segmentDurationIn180k && !(compatFlags & SegNumStartsAtZero)) {
			segmentNum = firstDataAbsTimeInMs / clockToTimescale(segmentDurationIn180k, 1000);
		}
		startSegment();
	}

	auto refData = std::dynamic_pointer_cast<const DataBaseRef>(data);
	return !(refData && !refData->getData());
}

void GPACMuxMP4::process() {
	auto data = inputs[0]->pop(); //FIXME: reimplement with multiple inputs
	if (!processInit(data))
		return;

	if (compatFlags & ExactInputDur) {
		if (lastData) {
			auto dataDurationInTs = clockToTimescale(data->getMediaTime() - initTimeIn180k, mediaTs) - DTS;
			if (dataDurationInTs <= 0) {
				dataDurationInTs = defaultSampleIncInTs;
				log(Warning, "Computed duration is inferior or equal to zero. Inferring to %s", defaultSampleIncInTs);
			}
			processSample(fillSample(lastData), dataDurationInTs);
		}

		lastData = data;
	} else {
		auto lastDataDurationInTs = clockToTimescale(data->getMediaTime() - initTimeIn180k, mediaTs) + defaultSampleIncInTs - DTS;
		if (DTS > 0) {
			if (!data->getMediaTime()) {
				lastDataDurationInTs = defaultSampleIncInTs;
				log(Warning, "Received time 0: inferring duration of %s", lastDataDurationInTs);
			}
			if (lastDataDurationInTs - defaultSampleIncInTs != 0) {
				if (lastDataDurationInTs <= 0) {
					lastDataDurationInTs = 1;
				}
				log(Debug, "VFR: adding sample with duration %ss", lastDataDurationInTs / (double)mediaTs);
			}
		}

		processSample(fillSample(data), lastDataDurationInTs);
	}
}

}
}
