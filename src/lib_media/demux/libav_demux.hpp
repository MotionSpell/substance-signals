#pragma once

#include "lib_modules/core/module.hpp"
#include "../common/libav.hpp"
#include <list>
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

		void dispatch();
		void setTime(std::shared_ptr<DataAVPacket> data, int streamIdx);
		std::vector<std::unique_ptr<Transform::Restamp>> restampers;
		std::vector<std::list<std::unique_ptr<DataAVPacket>>> dispatchPkts;

		struct AVFormatContext *m_formatCtx;
		int64_t startPTS = 0;
		std::vector<OutputDataDefault<DataAVPacket>*> outputs;
};

}
}
