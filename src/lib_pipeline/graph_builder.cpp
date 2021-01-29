#include "graph_builder.hpp"
#include "lib_utils/json.hpp"
#include "lib_utils/log.hpp" // g_Log
#include "lib_utils/small_map.hpp"
#include "lib_utils/tools.hpp" // uptr
#include <iostream>
#include <stdexcept>

namespace {
auto const JSON_VERSION = 1;
}

namespace Pipelines {

std::unique_ptr<Pipeline> createPipelineFromJSON(const std::string &jsonText, std::function<ParseModuleConfig> parseModuleConfig) {
	auto pipeline = uptr(new Pipeline);

	auto json = json::parse(jsonText);

	if (json.objectValue.find("version") == json.objectValue.end())
		return nullptr;

	auto const &version = json.objectValue["version"].intValue;
	if (version != JSON_VERSION) {
		auto const err = std::string("Config version is") + std::to_string(version) + ", expected " + std::to_string(JSON_VERSION);
		throw std::runtime_error(err.c_str());
	}

	SmallMap<std::string /*caption*/, IFilter*> modulesDesc;

	if (json.objectValue.find("modules") == json.objectValue.end())
		return nullptr;

	{
		auto &modules = json["modules"];
		for (auto const &module : modules.objectValue) {
			std::string moduleType, moduleCaption = module.key;
			SmallMap<std::string, json::Value> moduleConfig;

			for (auto const &prop : module.value.objectValue) {
				if (prop.key == "type") {
					moduleType = prop.value.stringValue;
				} else if (prop.key == "config") {
					moduleConfig = prop.value.objectValue;
				} else {
					auto const err = std::string("\"modules\" unknown member:") + prop.key;
					throw std::runtime_error(err.c_str());
				}
			}

			auto va = parseModuleConfig(moduleType, moduleConfig);
			modulesDesc[moduleCaption] = pipeline->add(moduleType.c_str(), va.get());
		}
	}

	if (json.objectValue.find("connections") != json.objectValue.end()) {
		auto &connections = json["connections"];
		for (auto const &conn : connections.arrayValue) {
			InputPin i(nullptr);
			OutputPin o(nullptr);
			auto first = true;
			for (auto const &prop : conn.objectValue) {
				if (first) {
					o.mod = modulesDesc[prop.key];
					o.index = prop.value.intValue;
					first = false;
				} else {
					i.mod = modulesDesc[prop.key];
					i.index = prop.value.intValue;
					pipeline->connect(o, i);
				}
			}
		}
	}

	return pipeline;
}

}
