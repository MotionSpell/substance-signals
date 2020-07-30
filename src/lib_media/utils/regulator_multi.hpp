#pragma once

/*
Feeds downstream modules with comparable media times.

This module delays dispatch until all data corresponding up to a media time is
available. Data won't be modified (e.g. added or removed). Ensure that backward
modules have sufficiently sized allocators.

Delaying compressed data takes way less memory than raw data. Thus this module
is complementary to rectifiers operating on raw data (e.g. in a downward module).

The module works this way:
 - At each data reception, evaluate if some data is dispatchable.
 - Non-dispatchable data is queued in a fifo.
 - We still rely on Clock times to handle drifts and discontinuities.
 - The different media types are processed differently (video&audio = continuous, subtitles = sparse).
*/
#include "lib_modules/utils/helper_dyn.hpp"
#include "lib_utils/clock.hpp" // timescaleToClock
#include "lib_utils/system_clock.hpp"
#include <algorithm> // max, remove_if
#include <cassert>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

namespace Modules {

struct RegulatorMulti : public ModuleDynI {
		RegulatorMulti(KHost* host, int maxMediaTimeDelayInMs, int maxClockTimeDelayInMs)
			: m_host(host),
			  maxMediaTimeDelay(timescaleToClock(maxMediaTimeDelayInMs, 1000)),
			  maxClockTimeDelay(timescaleToClock(maxClockTimeDelayInMs, 1000)) {
		}

		void flush() override {
			for (int i = 0; i < (int)streams.size(); ++i)
				for (auto &rec : streams[i])
					outputs[i]->post(rec.data);
		}

		void process() override {
			mimicOutputs();

			int id;
			auto data = popAny(id);
			assert(id < (int)streams.size());

			if (isDeclaration(data)) {
				outputs[id]->post(data);
				return;
			}

			auto const now = fractionToClock(g_SystemClock->now());
			streams[id].push_back({now, data});

			for (int i = 0; i < (int)streams.size(); ++i)
				if (data->getMetadata())
					if (data->getMetadata()->isAudio() || data->getMetadata()->isVideo())
						mediaDispatchTime = std::max<int64_t>(mediaDispatchTime, data->get<PresentationTime>().time - maxMediaTimeDelay);
			
			// Normal case
			dispatch([&](Rec const& rec) { return rec.data->get<DecodingTime>().time < mediaDispatchTime; });

			// Too old according to clock time: dispatch anyway
			dispatch([&](Rec const& rec) { return rec.creationTime < now - maxClockTimeDelay; });
		}

		int getNumOutputs() const override {
			{
				auto pThis = const_cast<RegulatorMulti*>(this);
				pThis->mimicOutputs();
			}
			return ModuleDynI::getNumOutputs();
		}

		IOutput* getOutput(int i) override {
			mimicOutputs();
			return ModuleDynI::getOutput(i);
		}

	private:
		struct Rec {
			int64_t creationTime;
			Data data;
		};
		struct Stream : std::vector<Rec> {};

		Data popAny(int& inputIdx) {
			Data data;
			inputIdx = 0;
			while (!inputs[inputIdx]->tryPop(data)) {
				inputIdx++;
			}
			return data;
		}

		void mimicOutputs() {
			while(streams.size() < getInputs().size()) {
				addOutput();
				streams.push_back({});
			}
		}

		void dispatch(std::function<bool(Rec const&)> predicate) {
			for (int i = 0; i < (int)streams.size(); ++i) {
				for (auto& rec : streams[i]) {
					if (predicate(rec)) {
						outputs[i]->post(rec.data);
						continue;
					}
				}

				//remove dispatched
				streams[i].erase(std::remove_if(streams[i].begin(), streams[i].end(), predicate), streams[i].end());
			}
		}

		KHost* const m_host;

		std::vector<Stream> streams;
		const int64_t maxMediaTimeDelay, maxClockTimeDelay;
		int64_t mediaDispatchTime = std::numeric_limits<int64_t>::min();
};

}
