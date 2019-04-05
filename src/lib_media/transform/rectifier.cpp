/*
Feeds downstream modules with a "clean" signal.

A "clean" signal has the following properties:
1) its timings are continuous (no gaps, overlaps, or discontinuities - but may not start at zero),
2) the different media are synchronized.

The module needs to be sample accurate.
It operates on raw data. Raw data requires a lot of memory, however:
1) we store a short duration (typically 500ms) and the framework works by default with pre-allocated pools,
2) RAM is cheap ;)

The module works this way:
 - At each tick it pulls some data (like a mux would).
 - We rely on Clock times. Media times are considered non-reliable and only used to achieve sync.
 - The different media types are processed differently (video = lead, audio = pulled, subtitles = sparse).
*/
#include "rectifier.hpp"

#include "lib_modules/modules.hpp"
#include "lib_modules/utils/helper_dyn.hpp"
#include "lib_utils/format.hpp"
#include "lib_utils/i_scheduler.hpp"
#include "lib_utils/log_sink.hpp"
#include "lib_utils/scheduler.hpp"
#include "lib_utils/tools.hpp" // enforce, safe_cast

#include "../common/pcm.hpp"

#include <cassert>
#include <memory>
#include <mutex>
#include <vector>
#include <algorithm> // remove_if

using namespace Modules;

namespace {

static auto const analyzeWindowIn180k = IClock::Rate * 500 / 1000; // 500 ms

struct Stream {
	struct Rec {
		int64_t creationTime;
		Data data;
	};

	OutputDefault* output;
	std::vector<Rec> data;
	Data blank {}; // when we have no data from the input, send this instead.
	PcmFormat fmt {};
};

// a time range, in clock units or audio sample units.
struct Interval {
	int64_t start, stop;
};

struct Rectifier : ModuleDynI {
		Rectifier(KHost* host, std::shared_ptr<IClock> clock_, IScheduler* scheduler_, Fraction frameRate)
			: m_host(host),
			  framePeriod(frameRate.inverse()),
			  clock(clock_),
			  scheduler(scheduler_) {
		}

		~Rectifier() {
			std::unique_lock<std::mutex> lock(inputMutex);
			if(m_pendingTaskId)
				scheduler->cancel(m_pendingTaskId);
		}

		void process() override {
			if(m_pendingTaskId == IScheduler::Id())
				reschedule(clock->now());
		}

		int getNumOutputs() const override {
			{
				auto pThis = const_cast<Rectifier*>(this);
				pThis->mimicOutputs();
			}
			return ModuleDynI::getNumOutputs();
		}

		IOutput* getOutput(int i) override {
			mimicOutputs();
			return ModuleDynI::getOutput(i);
		}

	private:

		KHost* const m_host;

		Fraction const framePeriod;
		int64_t numTicks = 0;
		std::vector<Stream> streams;
		std::mutex inputMutex;
		std::shared_ptr<IClock> clock;
		IScheduler* const scheduler;
		IScheduler::Id m_pendingTaskId {};

		void mimicOutputs() {
			while(streams.size() < getInputs().size()) {
				std::unique_lock<std::mutex> lock(inputMutex);
				auto output = addOutput<OutputDefault>();
				streams.push_back(Stream{output, {}});
			}
		}

		void reschedule(Fraction when) {
			m_pendingTaskId = scheduler->scheduleAt(std::bind(&Rectifier::onPeriod, this, std::placeholders::_1), when);
		}

		void onPeriod(Fraction timeNow) {
			m_pendingTaskId = {};
			emitOnePeriod(timeNow);
			{
				std::unique_lock<std::mutex> lock(inputMutex);
				reschedule(timeNow + framePeriod);
			}
		}

		void declareScheduler(IInput* input, IOutput* output) {
			auto const oMeta = output->getMetadata();
			if (!oMeta) {
				m_host->log(Debug, "Output isn't connected or doesn't expose a metadata: impossible to check.");
			} else if (input->getMetadata()->type != oMeta->type)
				throw error("Metadata I/O inconsistency");
		}

		void fillInputQueuesUnsafe() {
			auto now = fractionToClock(clock->now());

			for (auto i : getInputs()) {
				auto &currInput = inputs[i];
				Data data;
				while (currInput->tryPop(data)) {
					streams[i].data.push_back({now, data});
					if (currInput->updateMetadata(data)) {
						declareScheduler(currInput.get(), streams[i].output);
					}
				}
			}
		}

		Data chooseNextMasterFrame(Stream& stream, int64_t now) {
			if(stream.data.empty())
				return stream.blank;

			stream.blank = stream.data.front().data;

			// Introduce some latency.
			// if the frame is available, but since very little time, use it, but don't remove it.
			// Thus, it will be used again next time.
			// This protects us from frame phase changes (e.g on SDI cable replacement).
			if(abs(stream.data[0].creationTime - now) < fractionToClock(framePeriod))
				return stream.blank;

			auto r = stream.data.front().data;
			stream.data.erase(stream.data.begin());

			return r;
		}

		int getMasterStreamId() const {
			for(auto i : getInputs()) {
				if (inputs[i]->getMetadata() && inputs[i]->getMetadata()->type == VIDEO_RAW) {
					return i;
				}
			}
			return -1;
		}

		// post one "media period" on all outputs.
		//
		// In this function we distinguish between "in" media times, and "out" media times.
		//
		// - "in" media times are the media times of the input samples.
		// They might contain gaps, offsets, etc.
		// Thus, they should only be used for synchronisation between input streams.
		//
		// - "out" media times are the media times of the output samples.
		// They must be perfectly continuous, increasing, and must not depend in any way
		// of the input framing.
		void emitOnePeriod(Fraction now) {
			std::unique_lock<std::mutex> lock(inputMutex);
			fillInputQueuesUnsafe();
			discardOutdatedData(fractionToClock(now) - analyzeWindowIn180k);

			// output media times corresponding to the "media period"
			auto const outMasterTime = Interval {
				fractionToClock(Fraction((numTicks+0) * framePeriod.num, framePeriod.den)),
				fractionToClock(Fraction((numTicks+1) * framePeriod.num, framePeriod.den))
			};

			// input media times corresponding to the "media period"
			Interval inMasterTime {};

			auto const masterStreamId = getMasterStreamId();

			{
				if(masterStreamId == -1)
					throw error("No master stream: requires to have one video stream connected");
				auto& master = streams[masterStreamId];
				auto masterFrame = chooseNextMasterFrame(master, fractionToClock(now));
				if (!masterFrame) {
					assert(numTicks == 0);

					m_host->log(Warning, format("No available reference data for clock time %s", fractionToClock(now)).c_str());
					return;
				}

				inMasterTime.start = masterFrame->getMediaTime();
				inMasterTime.stop = inMasterTime.start + (outMasterTime.stop - outMasterTime.start);

				if (numTicks == 0) {
					m_host->log(Info, format("First available reference clock time: %s", fractionToClock(now)).c_str());
				}

				auto data = clone(masterFrame);
				data->setMediaTime(outMasterTime.start);
				master.output->post(data);
				discardStreamOutdatedData(masterStreamId, data->getMediaTime());
			}

			//TODO: Notes:
			//DO WE NEED TO KNOW IF WE ARE ON ERROR STATE? => LOG IT
			//23MS OF DESYNC IS OK => KEEP TRACK OF CURRENT DESYNC
			//AUDIO: BE ABLE TO ASK FOR A LARGER BUFFER ALLOCATOR? => BACK TO THE APP + DYN ALLOCATOR SIZE?
			//VIDEO: HAVE ONLY A FEW DECODED FRAMES: THEY ARRIVE IN ADVANCE ANYWAY
			for (auto i : getInputs()) {
				if(i == masterStreamId)
					continue;

				auto& input = inputs[i];

				if(!input->getMetadata())
					continue;

				switch (input->getMetadata()->type) {
				case AUDIO_RAW:
					emitOnePeriod_RawAudio(i, inMasterTime, outMasterTime);
					break;
				case VIDEO_RAW:
					throw error("only one video stream is supported");
					break;
				default: throw error("unhandled media type (awakeOnFPS)");
				}
			}

			++numTicks;
		}

		void emitOnePeriod_RawAudio(int i, Interval inMasterTime, Interval outMasterTime) {
			auto& stream = streams[i];

			if(!stream.data.empty())
				stream.fmt = safe_cast<const DataPcm>(stream.data.front().data)->format;

			// can't process data if we don't know the format
			if(stream.fmt.sampleRate == 0)
				return;

			auto const BPS = stream.fmt.getBytesPerSample() / stream.fmt.numPlanes;

			// convert a timestamp to an absolute sample count
			auto toSamples = [&](int64_t time) -> int64_t {
				return (time * stream.fmt.sampleRate) / IClock::Rate;
			};

			auto toSamplesP = [&](Interval p) -> Interval {
				return { toSamples(p.start), toSamples(p.stop) };
			};

			auto getSampleInterval = [&](const DataPcm* pcm) -> Interval {
				auto const start = toSamples(pcm->getMediaTime());
				return Interval {
					start,
					start + int64_t(pcm->getPlaneSize() / BPS)
				};
			};

			// convert all times to absolute sample counts.
			// This way we handle early all precision issues.
			// Doing computations in sample counts (instead of clock rate)
			// allows us to ensure sample accuracy.
			auto const outMasterSamples = toSamplesP(outMasterTime);
			auto const inMasterSamples = toSamplesP(inMasterTime);

			// Create an output sample. We want it to start at 'outMasterTime.start',
			// and to cover the full 'outMasterTime' interval.
			auto pcm = stream.output->allocData<DataPcm>(0);
			pcm->setMediaTime(outMasterTime.start);
			pcm->format = stream.fmt;
			pcm->setSampleCount(outMasterSamples.stop - outMasterSamples.start);

			// remove obsolete samples
			auto isObsolete = [&](Stream::Rec const& rec) {
				auto const inputData = safe_cast<const DataPcm>(rec.data);
				auto const inSamples = getSampleInterval(inputData.get());
				return inSamples.stop < inMasterSamples.start;
			};

			while(!stream.data.empty() && isObsolete(stream.data.front()))
				stream.data.erase(stream.data.begin());

			// fill the period "outMasterSamples" with portions of input audio samples
			// that intersect with the "media period".
			for(auto& data : stream.data) {
				auto const inputData = safe_cast<const DataPcm>(data.data);

				auto const inSamples = getSampleInterval(inputData.get());

				// intersect period of this data with the "media period"
				auto const left = std::max(inSamples.start, inMasterSamples.start);
				auto const right = std::min(inSamples.stop, inMasterSamples.stop);

				// intersection is empty
				if(left >= right)
					continue;

				for(int i=0; i < stream.fmt.numPlanes; ++i) {
					auto src = inputData->getPlane(i) + (left - inSamples.start) * BPS;
					auto dst = pcm->getPlane(i) + (left - inMasterSamples.start) * BPS;
					memcpy(dst, src, (right - left) * BPS);
				}
			}

			stream.output->post(pcm);
		}

		void discardOutdatedData(int64_t removalClockTime) {
			for (auto i : getInputs())
				discardStreamOutdatedData(i, removalClockTime);
		}

		void discardStreamOutdatedData(size_t inputIdx, int64_t removalClockTime) {
			auto isOutdated = [&](Stream::Rec const& rec) {
				return rec.creationTime < removalClockTime;
			};

			auto& data = streams[inputIdx].data;
			data.erase(std::remove_if(data.begin(), data.end(), isOutdated), data.end());
		}
};

IModule* createObject(KHost* host, void* va) {
	auto config = (RectifierConfig*)va;
	enforce(host, "Rectifier: host can't be NULL");
	enforce(config, "Rectifier: config can't be NULL");
	return createModuleWithSize<Rectifier>(100, host, config->clock, config->scheduler, config->frameRate).release();
}

auto const registered = Factory::registerModule("Rectifier", &createObject);
}
