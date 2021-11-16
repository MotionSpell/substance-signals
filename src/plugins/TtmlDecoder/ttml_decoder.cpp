#include "ttml_decoder.hpp"
#include "lib_utils/format.hpp"
#include "lib_utils/tools.hpp" // enforce
#include "lib_utils/log_sink.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_modules/utils/factory.hpp"
#include "lib_media/common/attributes.hpp"
#include "lib_media/common/metadata.hpp" //MetadataRawSubtitle
#include "lib_media/common/subtitle.hpp"
#include "lib_utils/sax_xml_parser.hpp"
#include "lib_utils/xml.hpp"
#include <cassert>

using namespace Modules;

namespace {
Tag parseXml(span<const char> text) {
	Tag root;
	std::vector<Tag*> tagStack = { &root };

	auto onNodeStart = [&](std::string name, SmallMap<std::string, std::string> &attr) {
		Tag tag{name};

		for (auto &a : attr)
			tag.attr.push_back({a.key, a.value});

		tagStack.back()->add(tag);
		tagStack.push_back(&tagStack.back()->children.back());
	};

	auto onNodeEnd = [&](std::string /*id*/, std::string content) {
		tagStack.back()->content = content;
		tagStack.pop_back();
	};

	saxParse(text, onNodeStart, onNodeEnd);

	assert(tagStack.front()->children.size() == 1);
	return tagStack.front()->children[0];
}

void explore(const Tag& tag, std::function<void(const Tag&)> processTag) {
	processTag(tag);

	for (auto &child: tag.children)
		explore(child, processTag);
}

class TTMLDecoder : public ModuleS {
	public:
		TTMLDecoder(KHost* host, TtmlDecoderConfig* cfg)
			: m_host(host), m_utcStartTime(cfg->utcStartTime) {
			enforce(cfg->utcStartTime != nullptr, "TTMLDecoder: utcStartTime can't be NULL");
			output = addOutput();
			output->setMetadata(make_shared<MetadataRawSubtitle>());
		}

		void processOne(Data data) override {
			auto document = parseXml({ (const char*)data->data().ptr, data->data().len });
			if(document.name != "tt" && document.name != "tt:tt")
				throw error("Not a TTML document");

			SmallMap<std::string, Page::Line/*empty content*/> styles;

			explore(document, [&](const Tag& tag) {
				if (tag.name == "style" || tag.name == "tt:style") {
					std::string id;
					Page::Line lineStyle;

					for (auto &attr : tag.attr) {
						if (attr.name == "xml:id")
							id = attr.value;
						else if (attr.name == "tts:fontFamily")
							m_host->log(Debug, format("Ignored attribute %s", attr.name).c_str());
						else if (attr.name == "tts:fontSize")
							m_host->log(Debug, format("Ignored attribute %s", attr.name).c_str());
						else if (attr.name == "tts:lineHeight")
							m_host->log(Debug, format("Ignored attribute %s", attr.name).c_str());
						else if (attr.name == "tts:color")
							lineStyle.color = attr.value;
						else if (attr.name == "tts:backgroundColor")
							m_host->log(Debug, format("Ignored attribute %s", attr.name).c_str());
						else if (attr.name == "tts:textAlign")
							m_host->log(Debug, format("Ignored attribute %s", attr.name).c_str());
						else
							m_host->log(Warning, format("Unknown attribute %s: please report to your vendor", attr.name).c_str());
					}

					styles[id] = lineStyle;
				}
			});

			Page page;
			explore(document, [&](const Tag& tag) {
				if (tag.name == "span" || tag.name == "tt:span") {
					Page::Line line;

					// find style
					for (auto &attr : tag.attr)
						if (attr.name == "style")
							line = styles[attr.value];
						else if (attr.name == "tts:color")
							line.color = attr.value;

					line.text = tag.content;
					page.lines.push_back(line);
				}
			});

			if (!page.lines.empty())
				sendSample(page);
		}

		void sendSample(const Page& page) {
			auto out = output->allocData<DataSubtitle>(0);
			out->setMediaTime(page.showTimestamp);
			out->page = page;

			CueFlags flags {};
			flags.keyframe = true;
			out->set(flags);

			output->post(out);
		}

	private:
		KHost* const m_host;
		IUtcStartTimeQuery const * const m_utcStartTime;
		OutputDefault* output;
};

IModule* createObject(KHost* host, void* va) {
	auto config = (TtmlDecoderConfig*)va;
	enforce(host, "TTMLDecoder: host can't be NULL");
	enforce(config, "TTMLDecoder: config can't be NULL");
	return createModule<TTMLDecoder>(host, config).release();
}

auto const registered = Factory::registerModule("TTMLDecoder", &createObject);
}
