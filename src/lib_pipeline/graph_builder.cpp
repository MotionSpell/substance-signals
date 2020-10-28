#include "graph_builder.hpp"
#include "lib_utils/log.hpp" // g_Log
#include "lib_utils/tools.hpp" // uptr
#include "rapidjson/error/en.h"
#include <rapidjson/document.h>
#include <iostream>
#include <stdexcept>

using namespace rapidjson;

namespace {
auto const JSON_VERSION = 1;
}

namespace Pipelines {

std::unique_ptr<Pipeline> createPipelineFromJSON(std::string json, std::function<ParseModuleConfig> parseModuleConfig) {
	auto pipeline = uptr(new Pipeline);

	Document d;
	d.Parse(json.c_str());
	if (d.HasParseError()) {
		std::cerr << "Error (offset " << static_cast<unsigned>(d.GetErrorOffset()) << "): " << GetParseError_En(d.GetParseError()) << "%s" << std::endl;
		throw std::runtime_error(std::string("Invalid JSON:\n") + json);
	}

	auto const &version = d["version"];
	if (version.GetUint() != JSON_VERSION) {
		auto const err = std::string("Config version is") + std::to_string(version.GetUint()) + ", expected " + std::to_string(JSON_VERSION);
		throw std::runtime_error(err.c_str());
	}

	SmallMap<std::string /*caption*/, IFilter*> modulesDesc;

	{
		auto &modules = d["modules"];
		for (auto const &module : modules.GetObject()) {
			std::string moduleType, moduleCaption = module.name.GetString();
			SmallMap<std::string, std::string> moduleParams;

			for (auto const &prop : module.value.GetObject()) {
				if (prop.name.GetString() == std::string("type")) {
					moduleType = prop.value.GetString();
				} else if (prop.name.GetString() == std::string("config")) {
					for (auto const &params : prop.value.GetArray())
						for (auto const &param : params.GetObject())
							moduleParams[param.name.GetString()] = param.value.GetString();
				}
			}

			auto va = parseModuleConfig(moduleType, moduleParams);
			modulesDesc[moduleCaption] = pipeline->add(moduleType.c_str(), va.get());
		}
	}

	{
		auto &connections = d["connections"];
		for (auto const &conn : connections.GetArray()) {
			InputPin i(nullptr);
			OutputPin o(nullptr);
			bool first = true;
			for (auto const &prop : conn.GetObject()) {
				if (first) {
					o.mod = modulesDesc[prop.name.GetString()];
					o.index = prop.value.GetUint();
					first = false;
				} else {
					i.mod = modulesDesc[prop.name.GetString()];
					i.index = prop.value.GetUint();
					pipeline->connect(o, i);
				}
			}
		}
	}

	return pipeline;
}

}
