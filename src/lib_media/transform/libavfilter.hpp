#pragma once

#include "../common/picture.hpp"

struct AvFilterConfig {
	Modules::PictureFormat format;
	std::string filterArgs;
};

#include "lib_modules/utils/helper.hpp"
#include "../common/ffpp.hpp"
#include <string>

struct AVFilter;
struct AVFilterGraph;
struct AVFilterContext;

namespace Modules {
namespace Transform {

class LibavFilter : public ModuleS {
	public:
		LibavFilter(IModuleHost* host, const AvFilterConfig& cfg);
		~LibavFilter();
		void process(Data data) override;

	private:
		IModuleHost* const m_host;
		AVFilterGraph *graph;
		AVFilterContext *buffersrc_ctx, *buffersink_ctx;
		std::unique_ptr<ffpp::Frame> const avFrameIn, avFrameOut;
};

}
}
