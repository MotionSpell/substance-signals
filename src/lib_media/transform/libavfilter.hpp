#pragma once

#include "lib_modules/core/module.hpp"
#include "lib_ffpp/ffpp.hpp"
#include "../common/picture.hpp"
#include <string>

struct AVFilter;
struct AVFilterGraph;
struct AVFilterContext;

namespace Modules {
namespace Transform {

class LibavFilter : public ModuleS {
public:
	LibavFilter(const PictureFormat &format, const std::string &filterArgs);
	~LibavFilter();
	void process(Data data) override;

private:
	AVFilterGraph *graph;
	AVFilterContext *buffersrc_ctx, *buffersink_ctx;
	std::unique_ptr<ffpp::Frame> const avFrameIn, avFrameOut;
	Signals::Queue<uint64_t> times;
};

}
}
