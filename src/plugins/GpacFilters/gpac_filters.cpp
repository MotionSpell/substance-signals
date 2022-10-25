#include "gpac_filters.hpp"
#include "lib_media/common/attributes.hpp"
#include "lib_modules/utils/factory.hpp"
#include "lib_modules/utils/helper_dyn.hpp"
#include "lib_utils/log_sink.hpp"
#include "lib_utils/tools.hpp" // safe_cast, enforce
#include "lib_utils/format.hpp"
#include "lib_media/common/metadata.hpp"
#include "lib_utils/queue.hpp"
#include "lib_utils/small_map.hpp"
#include <cassert>

extern "C" {
#include "gpac_filter_mem_in.h"
#include "gpac_filter_mem_out.h"
}

//#define GPAC_V2

using namespace Modules;

namespace {
struct GpacFilters : ModuleDynI {
		GpacFilters(KHost* host, GpacFiltersConfig *cfg)
			: m_host(host), filterName(cfg->filterName) {
			gf_sys_init(GF_MemTrackerNone, NULL);
			//gf_log_set_tools_levels("all@info:media@debug", GF_TRUE);
		}

		~GpacFilters() {
			//TODO
		}

		void process() override {
			int inputIdx = 0;
			Data data;
			while (!inputs[inputIdx]->tryPop(data))
				inputIdx++;

			ioDiff++;
			if (ioDiff > maxIoDiff) {
				m_host->log(Error, "reframer seems stuck - resetting");
				ioDiff = 0;
				fs = nullptr; //Romain: leaks
			}

			if(!fs) {
				auto meta = data->getMetadata();
				if (!meta)
					throw error("Can't instantiate reframer: no metadata for input data");

				mimicOutputs();

				if (!openReframer(meta))
					return;
			}

			inputData.push(data);
#ifdef GPAC_V2
			do {
				for (int i=0; i<10; ++i)
					gf_fs_run(fs);
			} while (0);//Romain: !gf_fs_is_last_task(fs));
#else
			for (int i=0; i<10; ++i)
				gf_fs_run_step(fs);
#endif

			//gf_fs_print_connections(fs);
			//gf_fs_print_all_connections(fs, (char*)"mem_in", nullptr);
		}

		void flush() override {
			auto e = gf_fs_abort(fs, GF_FS_FLUSH_ALL);
			if (e != GF_OK)
				m_host->log(Warning, format("Flush failed: some data may be missing (%s)", gf_error_to_string(e)).c_str());

			if (fs)
#ifdef GPAC_V2
				do {
					gf_fs_run(fs);
				} while (!gf_fs_is_last_task(fs));
#else
				for (int i=0; i<100; ++i)
					gf_fs_run_step(fs);
#endif

			int remaining = 0;
			Data data;
			while (inputData.tryPop(data))
				remaining++;
			if (remaining)
				m_host->log(Warning, format("%s packets were unprocessed", remaining).c_str());
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
		int ioDiff = 0;
		const int maxIoDiff = 20;

		//memIn
		Queue<Data> inputData;
		SmallMap<const u8*, Data> danglingData;
		static void inputGetData(void *parent, const u8 **data, u32 *data_size, u64 *dts, u64 *pts) {
			auto pThis = (GpacFilters*)parent;
			Data pData = nullptr;
			if (pThis->inputData.tryPop(pData)) {
				auto span = pData->data();
				*data = span.ptr;
				*data_size = span.len;
				try {
					*pts = pData->get<PresentationTime>().time;
				} catch(...) {
					*pts = 0; // not set
				}
				try {
					*dts = pData->get<DecodingTime>().time;
				} catch(...) {
					*dts = *pts;
				}
				pThis->danglingData[*data] = pData;
			} else {
				//pThis->m_host->log(Debug, "MemIn requests data but no data is available. Rescheduling.");
			}
		}
		static void inputFreeData(void *parent, const u8 *data) {
			auto pThis = (GpacFilters*)parent;
			pThis->danglingData.erase(pThis->danglingData.find(data));
		}

		//memOut
		std::string codecName;
		static void outputPushData(void *parent, const u8 *data, u32 data_size, u64 dts, u64 pts) {
			auto pThis = (GpacFilters*)parent;
			pThis->ioDiff = 0;

			auto out = ((OutputDefault*)((GpacFilters*)pThis)->outputs[0].get())->allocData<DataRaw>(data_size); //TODO: to be extended to multiple outputs
			out->set(DecodingTime{ (int64_t)dts });
			out->set(PresentationTime { (int64_t)pts });
			out->set(CueFlags{ false, true, false });
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

		bool openReframer(Metadata meta_) {
			auto meta = safe_cast<const MetadataPkt>(meta_);
			outputs[0]->setMetadata(meta); //TODO: to be extended to multiple outputs
			codecName = meta->codec;
			if (codecName.empty())
				return false;

			fs = gf_fs_new(1, GF_FS_SCHEDULER_DIRECT, GF_FS_FLAG_NO_MAIN_THREAD, NULL);
			if (!fs)
				throw error("cannot create GPAC Filters session");

#ifdef GPAC_V2
			fs = gf_fs_new_defaults(GF_FS_FLAG_NO_BLOCKING/*Romain: GF_FS_FLAG_NON_BLOCKING*/);
			if (!fs)
				throw error("cannot set GPAC Filters session non-blocking flag");
#endif

			gf_fs_add_filter_register(fs, mem_in_register(fs));
			gf_fs_add_filter_register(fs, mem_out_register(fs));

			{
				GF_Err e = GF_OK;
				memIn = gf_fs_load_source(fs, "signals://memin/", "FID=MEMIN", nullptr, &e);
				if (e)
					throw error(format("cannot load create GPAC Filters source: %s", gf_error_to_string(e)));
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
					throw error(format("cannot load create GPAC Filters sink: %s", gf_error_to_string(e)));
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
					throw error(format("cannot load create GPAC Filters \"%s\": %s", filterName, gf_error_to_string(e)));
			}

			return true;
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
