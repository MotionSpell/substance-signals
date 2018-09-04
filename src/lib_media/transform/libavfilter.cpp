#include "libavfilter.hpp"
#include "lib_utils/tools.hpp"
#include "../common/libav.hpp"

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

namespace Modules {
namespace Transform {

LibavFilter::LibavFilter(IModuleHost* host, const PictureFormat &format, const std::string &filterArgs)
	: m_host(host), graph(avfilter_graph_alloc()), avFrameIn(new ffpp::Frame), avFrameOut(new ffpp::Frame) {
	char args[512];
	AVPixelFormat pf = pixelFormat2libavPixFmt(format.format);
	snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d", format.res.width, format.res.height, pf, (int)IClock::Rate, 1, format.res.width, format.res.height);

	auto ret = avfilter_graph_create_filter(&buffersrc_ctx, avfilter_get_by_name("buffer"), "in", args, nullptr, graph);
	if (ret < 0)
		throw error("Cannot create filter source");

	ret = avfilter_graph_create_filter(&buffersink_ctx, avfilter_get_by_name("buffersink"), "out", nullptr, nullptr, graph);
	if (ret < 0)
		throw error("Cannot create filter destination");

	AVFilterInOut *outputs = avfilter_inout_alloc();
	outputs->name = av_strdup("in");
	outputs->filter_ctx = buffersrc_ctx;
	outputs->pad_idx = 0;
	outputs->next = nullptr;

	AVFilterInOut *inputs = avfilter_inout_alloc();
	inputs->name = av_strdup("out");
	inputs->filter_ctx = buffersink_ctx;
	inputs->pad_idx = 0;
	inputs->next = nullptr;

	ret = avfilter_graph_parse(graph, filterArgs.c_str(), inputs, outputs, NULL);
	if (ret < 0)
		throw error("Cannot parse filter graph");

	ret = avfilter_graph_config(graph, NULL);
	if (ret < 0)
		throw error("Cannot config filter graph");

	auto input = addInput(this);
	input->setMetadata(make_shared<MetadataRawVideo>());
	addOutput<OutputPicture>(make_shared<MetadataRawVideo>());
}

LibavFilter::~LibavFilter() {
	avfilter_graph_free(&graph);
}

void LibavFilter::process(Data data) {
	const auto pic = safe_cast<const DataPicture>(data);
	avFrameIn->get()->pict_type = AV_PICTURE_TYPE_NONE;
	avFrameIn->get()->format = (int)pixelFormat2libavPixFmt(pic->getFormat().format);
	for (size_t i = 0; i < pic->getNumPlanes(); ++i) {
		avFrameIn->get()->width = pic->getFormat().res.width;
		avFrameIn->get()->height = pic->getFormat().res.height;
		avFrameIn->get()->data[i] = (uint8_t*)pic->getPlane(i);
		avFrameIn->get()->linesize[i] = (int)pic->getPitch(i);
	}
	avFrameIn->get()->pts = data->getMediaTime();

	if (av_buffersrc_add_frame_flags(buffersrc_ctx, avFrameIn->get(), AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
		throw error("Error while feeding the filtergraph");

	while (1) {
		auto const ret = av_buffersink_get_frame(buffersink_ctx, avFrameOut->get());
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			break;
		}
		if (ret < 0) {
			m_host->log(Error, "Corrupted filter video frame.");
		}
		auto output = safe_cast<OutputPicture>(outputs[0].get());
		auto pic = DataPicture::create(output, Resolution(avFrameIn->get()->width, avFrameIn->get()->height), libavPixFmt2PixelFormat((AVPixelFormat)avFrameIn->get()->format));
		copyToPicture(avFrameOut->get(), pic.get());
		pic->setMediaTime(av_rescale_q(avFrameOut->get()->pts, buffersink_ctx->inputs[0]->time_base, buffersrc_ctx->outputs[0]->time_base));
		outputs[0]->emit(pic);
		av_frame_unref(avFrameOut->get());
	}
	av_frame_unref(avFrameIn->get());
}

}
}
