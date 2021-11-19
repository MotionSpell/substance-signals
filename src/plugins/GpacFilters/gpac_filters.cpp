#include "gpac_filters.hpp"
#include "lib_media/common/attributes.hpp"
#include "lib_modules/utils/factory.hpp"
#include "lib_modules/utils/helper_dyn.hpp"
#include "lib_utils/log_sink.hpp"
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
struct GpacFilters : ModuleDynI {
		GpacFilters(KHost* host, GpacFiltersConfig *cfg)
			: m_host(host), filterName(cfg->filterName) {
			gf_sys_init(GF_MemTrackerNone, NULL);
		}

		void process() override {
			int inputIdx = 0;
			Data data;
			while (!inputs[inputIdx]->tryPop(data))
				inputIdx++;

			if(!fs) {
				auto meta = data->getMetadata();
				if (!meta)
					throw error("Can't instantiate reframer: no metadata for input data");

				mimicOutputs();

				openReframer(meta);
			}

			inputData.push(data);
			gf_fs_run_step(fs);

			//gf_fs_print_connections(fs);
			//gf_fs_print_all_connections(fs, (char*)"mem_in", nullptr);
		}

		void flush() override {
			//TODO: better understand EOS mechanism in GPAC Filters
			if (fs)
				gf_fs_run_step(fs);
		}

		int getNumOutputs() const override {
			auto pThis = const_cast<GpacFilters*>(this);
			pThis->mimicOutputs();
			return ModuleDynI::getNumOutputs();
		}

		IOutput* getOutput(int i) override {
			mimicOutputs();
			return ModuleDynI::getOutput(i);
		}

	private:
		KHost* const m_host;
		std::string filterName;
		GF_FilterSession *fs = nullptr;
		GF_Filter *memIn = nullptr, *memOut = nullptr;

		//memIn
		Queue<Data> inputData;
		Data inputLast;
		static void inputGetData(void *parent, const u8 **data, u32 *data_size, u64 *dts, u64 *pts) {
			auto pThis = (GpacFilters*)parent;

			if (pThis->inputLast)
				return; /*pending packet: don't unqueue until the previous data is freed*/

			Data pData = nullptr;
			if (pThis->inputData.tryPop(pData)) {
				auto span = pData->data();
				*data = span.ptr;
				*data_size = span.len;
				*pts = pData->get<PresentationTime>().time;
				try {
					*dts = pData->get<DecodingTime>().time;
				} catch(...) {
					*dts = *pts;
				}
				pThis->inputLast = pData;
			}
		}
		static void inputFreeData(void *parent) {
			auto pThis = (GpacFilters*)parent;
			pThis->inputLast = nullptr;
		}

		//memOut
		std::string codecName;
		static void outputPushData(void *parent, const u8 *data, u32 data_size, u64 dts, u64 pts) {
			auto pThis = (GpacFilters*)parent;
			auto out = ((OutputDefault*)((GpacFilters*)pThis)->outputs[0].get())->allocData<DataRaw>(data_size); //TODO: to be extended to multiple outputs
			out->set(DecodingTime{ (int64_t)dts });
			out->set(PresentationTime { (int64_t)pts });
			out->set(CueFlags{false, true});
			memcpy(out->buffer->data().ptr, data, data_size);
			pThis->outputs[0]->post(out); //TODO: to be extended to multiple outputs
		}
		static void ensureMetadata(void *parent, const u8 *data, u32 data_size) {
			auto pThis = (GpacFilters*)parent;
			auto meta = safe_cast<const MetadataPkt>(pThis->outputs[0]->getMetadata());
			if(!meta || meta->codecSpecificInfo.empty()) {
				auto metaDsi = make_shared<MetadataPkt>(meta->type);
				metaDsi->codecSpecificInfo.assign(data, data + data_size);
				metaDsi->codec = pThis->codecName;
				pThis->outputs[0]->setMetadata(metaDsi);
			}
		}

		void openReframer(Metadata meta_) {
			auto meta = safe_cast<const MetadataPkt>(meta_);
			outputs[0]->setMetadata(meta); //TODO: to be extended to multiple outputs
			codecName = meta->codec;

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
				ctx->signals_codec_name = codecName.c_str();
				ctx->getData = &GpacFilters::inputGetData;
				ctx->freeData = &GpacFilters::inputFreeData;
			}

			{
				GF_Err e = GF_OK;
				memOut = gf_fs_load_destination(fs, "signals://memout/", "SID=PROCESSOR", nullptr, &e);
				if (e)
					throw error(format("cannot load create GPAC Filters sink: %s", gf_error_to_string(e)).c_str());
				auto ctx = (MemOutCtx*)gf_filter_get_udta(memOut);
				ctx->parent = (void*)this;
				ctx->pushData = &GpacFilters::outputPushData;
				ctx->pushDsi = &GpacFilters::ensureMetadata;
			}

			{
				GF_Err e = GF_OK;
				auto name = filterName + ":SID=MEMIN:FID=PROCESSOR";
				gf_fs_load_filter(fs, name.c_str(), &e);
				if (e)
					throw error(format("cannot load create GPAC Filters \"%s\": %s", filterName, gf_error_to_string(e)).c_str());
			}
		}

		void mimicOutputs() {
			while(ModuleDynI::getNumOutputs() < (int)getInputs().size()) {
				auto output = addOutput();
				output->setMetadata(getInput(ModuleDynI::getNumOutputs()-1)->getMetadata());
			}
		}
};

IModule* createObject(KHost* host, void* va) {
	auto config = (GpacFiltersConfig*)va;
	enforce(host, "GpacFilters: host can't be NULL");
	enforce(config, "GpacFilters: config can't be NULL");
	return createModuleWithSize<GpacFilters>(384, host, config).release();
}

auto const registered = Factory::registerModule("GpacFilters", &createObject);
}
