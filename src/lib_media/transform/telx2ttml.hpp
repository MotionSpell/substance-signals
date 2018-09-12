#pragma once

#include "lib_modules/utils/helper.hpp"
#include "../common/libav.hpp"
#include <vector>

namespace Modules {
namespace Transform {

struct Page;

struct ITelxConfig {
	virtual ~ITelxConfig() {}
};

struct TeletextToTtmlConfig {
	enum TimingPolicy {
		AbsoluteUTC,     //USP
		RelativeToMedia, //14496-30
		RelativeToSplit  //MSS
	};

	unsigned pageNum;
	std::string lang;
	uint64_t splitDurationInMs;
	uint64_t maxDelayBeforeEmptyInMs;
	TimingPolicy timingPolicy;
	std::function<int64_t()> getUtcPipelineStartTime = []() {
		return 0;
	};
};

class TeletextToTTML : public ModuleS {
	public:
		enum TimingPolicy {
			AbsoluteUTC,     //USP
			RelativeToMedia, //14496-30
			RelativeToSplit  //MSS
		};

		TeletextToTTML(IModuleHost* host, TeletextToTtmlConfig* cfg);
		virtual ~TeletextToTTML();
		void process(Data data) override;

	private:
		const std::string toTTML(uint64_t startTimeInMs, uint64_t endTimeInMs);
		void sendSample(const std::string &sample);
		void processTelx(DataAVPacket const * const pkt);
		void dispatch();

		IModuleHost* const m_host;
		std::function<int64_t()> getUtcPipelineStartTime;
		OutputDataDefault<DataAVPacket> *output;
		const unsigned pageNum;
		std::string lang;
		const TeletextToTtmlConfig::TimingPolicy timingPolicy;
		int64_t intClock = 0, extClock = 0;
		const uint64_t maxPageDurIn180k, splitDurationIn180k;
		uint64_t firstDataAbsTimeInMs = 0;
		std::vector<std::unique_ptr<Page>> currentPages;
		std::unique_ptr<ITelxConfig> config;
};

}
}
