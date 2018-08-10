#pragma once

#include "lib_modules/core/log.hpp"
#include "lib_modules/utils/helper.hpp"
#include "../common/libav.hpp"
#include <vector>
#include <iosfwd> // stringstream

namespace Modules {
namespace Transform {

struct Page {
	Page();
	const std::string toString() const;
	const std::string toTTML(uint64_t startTimeInMs, uint64_t endTimeInMs, uint64_t idx) const;
	const std::string toSRT();

	uint64_t tsInMs=0, startTimeInMs=0, endTimeInMs=0, showTimestamp=0, hideTimestamp=0;
	uint32_t framesProduced = 0;
	std::vector<std::unique_ptr<std::stringstream>> lines;
	std::stringstream *ss = nullptr;
};

struct ITelxConfig {
	virtual ~ITelxConfig() {}
};

class TeletextToTTML : public ModuleS, private LogCap {
	public:
		enum TimingPolicy {
			AbsoluteUTC,     //USP
			RelativeToMedia, //14496-30
			RelativeToSplit  //MSS
		};

		TeletextToTTML(unsigned pageNum, const std::string &lang, uint64_t splitDurationIn180k, uint64_t maxDelayBeforeEmptyInMs, TimingPolicy timingPolicy);
		void process(Data data) override;

	private:
		const std::string toTTML(uint64_t startTimeInMs, uint64_t endTimeInMs);
		void sendSample(const std::string &sample);
		void processTelx(DataAVPacket const * const pkt);
		void dispatch();

		OutputDataDefault<DataAVPacket> *output;
		const unsigned pageNum;
		std::string lang;
		const TimingPolicy timingPolicy;
		int64_t intClock = 0, extClock = 0;
		const uint64_t maxPageDurIn180k, splitDurationIn180k;
		uint64_t firstDataAbsTimeInMs = 0;
		std::vector<std::unique_ptr<Page>> currentPages;
		std::unique_ptr<ITelxConfig> config;
};

}
}
