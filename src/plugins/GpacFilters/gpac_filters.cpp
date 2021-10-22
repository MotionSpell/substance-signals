#include "gpac_filters.hpp"
#include "lib_media/common/attributes.hpp"
#include "lib_modules/utils/factory.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_utils/tools.hpp" // safe_cast, enforce
#include "lib_utils/format.hpp"
#include "lib_media/common/metadata.hpp"
#include "lib_utils/queue.hpp"
#include <cassert>

extern "C" {
#include "gpac_filter_mem_in.h"
#include "gpac_filter_mem_out.h"
}

using namespace Modules;

namespace {
struct GpacFilters : ModuleS {
		GpacFilters(KHost* host, GpacFiltersConfig */*cfg*/)
			: m_host(host) {
			output = addOutput();
			output->setMetadata(make_shared<MetadataPkt>(AUDIO_PKT));
		}

		void processOne(Data data) override {
			if(!fs) {
				auto meta = data->getMetadata();
				if (!meta)
					throw error("Can't instantiate reframer: no metadata for input data");

				openReframer(meta);
			}

			inputData.push(data);
			gf_fs_run_step(fs);
			gf_fs_print_connections(fs);
			gf_fs_print_all_connections(fs, (char*)"mem_in", nullptr);
		}

		void flush() override {
			if (fs)
				gf_fs_run_step(fs);
		}

	private:
		KHost* const m_host;
		OutputDefault* output;

		GF_FilterSession *fs;
		GF_Filter *memIn, *memOut;

		//memIn
		Queue<Data> inputData;
		Data inputLast;
		static void inputGetData(void *parent, const u8 **data, u32 *data_size, u64 *dts) {
			auto pThis = (GpacFilters*)parent;

			if (pThis->inputLast)
				return; /*pending packet: don't unqueue until the previous data is freed*/

			Data pData = nullptr;
			if (pThis->inputData.tryPop(pData)) {
				auto span = pData->data();
				*data = span.ptr;
				*data_size = span.len;
				*dts = pData->get<DecodingTime>().time;
				pThis->inputLast = pData;
			}
		}
		static void inputFreeData(void *parent) {
			auto pThis = (GpacFilters*)parent;
			pThis->inputLast = nullptr;
		}

		//memOut
		std::string codecName;
		static void outputPushData(void *parent, const u8 *data, u32 data_size, u64 dts) {
			auto pThis = (GpacFilters*)parent;
			auto out = pThis->output->allocData<DataRaw>(data_size);
			out->set(DecodingTime{ (int64_t)dts });
			out->set(PresentationTime { (int64_t)dts });
			out->set(CueFlags{false, true});
			memcpy(out->buffer->data().ptr, data, data_size);
			pThis->output->post(out);
		}
		static void ensureMetadata(void *parent, const u8 *data, u32 data_size) {
			auto pThis = (GpacFilters*)parent;
			auto meta = safe_cast<const MetadataPkt>(pThis->output->getMetadata());
			if(!meta || meta->codecSpecificInfo.empty()) {
				auto metaDsi = make_shared<MetadataPkt>(AUDIO_PKT);
				metaDsi->codecSpecificInfo.assign(data, data + data_size);
				metaDsi->codec = pThis->codecName;
				pThis->output->setMetadata(metaDsi);
			}
		}

		void openReframer(Metadata meta_) {
			auto meta = safe_cast<const MetadataPkt>(meta_);
			if (!meta->isAudio())
				throw error("non-audio input: unsupported");
			output->setMetadata(meta);
			codecName = meta->codec;

			gf_sys_init(GF_MemTrackerNone, NULL);
			fs = gf_fs_new(1, GF_FS_SCHEDULER_DIRECT, GF_FS_FLAG_NO_MAIN_THREAD, NULL);
			if (!fs)
				throw error("cannot create GPAC Filters session");

			gf_fs_add_filter_register(fs, mem_in_register(fs));
			gf_fs_add_filter_register(fs, mem_out_register(fs));

			{
				GF_Err e = GF_OK;
				memIn = gf_fs_load_source(fs, "signals://memin/", "FID=MEMIN", nullptr, &e);
				if (e)
					throw error(format("cannot load create GPAC Filters source: %s", gf_error_to_string(e)).c_str());
				auto ctx = (MemInCtx*)gf_filter_get_udta(memIn);
				ctx->parent = (void*)this;
				ctx->getData = &GpacFilters::inputGetData;
				ctx->freeData = &GpacFilters::inputFreeData;
			}

			{
				GF_Err e = GF_OK;
				memOut = gf_fs_load_destination(fs, "signals://memout/", "SID=REFRAMER", nullptr, &e);
				if (e)
					throw error(format("cannot load create GPAC Filters sink: %s", gf_error_to_string(e)).c_str());
				auto ctx = (MemOutCtx*)gf_filter_get_udta(memOut);
				ctx->parent = (void*)this;
				ctx->pushData = &GpacFilters::outputPushData;
				ctx->pushDsi = &GpacFilters::ensureMetadata;
			}

			{
				std::string reframer;
				if (codecName == "mp1" || codecName == "mp2")
					reframer = "rfmp3";
				else if (codecName ==  "aac_adts")
					reframer = "rfadts";
				else if (codecName ==  "aac_latm")
					reframer = "rflatm";
				else if (codecName == "ac3" || codecName == "eac3")
					reframer = "rfac3";
				else
					throw error(format("unsupported codec: %s", codecName).c_str());

				GF_Err e = GF_OK;
				auto name = reframer + ":SID=MEMIN:FID=REFRAMER";
				gf_fs_load_filter(fs, name.c_str(), &e);
				if (e)
					throw error(format("cannot load create GPAC Filters reframer: %s", gf_error_to_string(e)).c_str());
			}
		}
};

IModule* createObject(KHost* host, void* va) {
	auto config = (GpacFiltersConfig*)va;
	enforce(host, "GpacFilters: host can't be NULL");
	//enforce(config, "GpacFilters: config can't be NULL");
	return createModule<GpacFilters>(host, config).release();
}

auto const registered = Factory::registerModule("GpacFilters", &createObject);
}
