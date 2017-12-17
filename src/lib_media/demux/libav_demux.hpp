#pragma once

#include "lib_modules/core/module.hpp"
#include "../common/libav.hpp"
#include "lib_ffpp/ffpp.hpp"
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
	LibavDemux(const std::string &url, const bool loop = false, const std::string &avformatCustom = "", const uint64_t seekTimeInMs = 0, const std::string &formatName = "");
	~LibavDemux();
	void process(Data data) override;

private:
	void webcamList();
	bool webcamOpen(const std::string &options);
	void initRestamp();
	void seekToStart();
	bool rectifyTimestamps(AVPacket &pkt);
	void threadProc();
	void setMediaTime(std::shared_ptr<DataAVPacket> data);
	bool dispatchable(AVPacket * const pkt);
	void dispatch(AVPacket *pkt);
	void sparseStreamsHeartbeat(AVPacket const * const pkt);

	bool loop;
	std::thread workingThread;
	std::atomic_bool done;
	QueueLockFree<AVPacket> dispatchPkts;
	std::vector<std::unique_ptr<Transform::Restamp>> restampers;
	std::vector<OutputDataDefault<DataAVPacket>*> outputs;
	struct AVFormatContext *m_formatCtx;
	std::unique_ptr<ffpp::IAvIO> m_avio = nullptr;
	int64_t curTimeIn180k = 0, startPTSIn180k = 0;
	std::vector<uint64_t> offsetIn180k;
	std::vector<int64_t> lastDTS;
};

}
}
