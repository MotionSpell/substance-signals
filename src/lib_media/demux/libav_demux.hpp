#pragma once

#include "lib_modules/core/module.hpp"
#include "../common/libav.hpp"
#include <atomic>
#include <vector>

struct AVFormatContext;

namespace Modules {

namespace Transform {
	class Restamp;
}

namespace Demux {

class LibavDemux : public ModuleS {
	public:
		//@param url may be a file, a remote URL, or a webcam (set "webcam" to list the available devices)
		LibavDemux(const std::string &url, const uint64_t seekTimeInMs = 0);
		~LibavDemux();
		void process(Data data) override;

	private:
		void webcamList();
		bool webcamOpen(const std::string &options);
		void threadProc();
		void setTime(std::shared_ptr<DataAVPacket> data);
		void dispatch(AVPacket *pkt);

		std::thread workingThread;
		std::atomic_bool done;
		Signals::QueueLockFree<AVPacket*> dispatchPkts;
		std::vector<std::unique_ptr<Transform::Restamp>> restampers;
		std::vector<OutputDataDefault<DataAVPacket>*> outputs;
		struct AVFormatContext *m_formatCtx;
		int64_t startPTS = 0;
};

}
}
