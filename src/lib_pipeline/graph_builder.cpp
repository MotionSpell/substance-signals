#include "graph_builder.hpp"
#include "lib_utils/json.hpp"
#include "lib_utils/log.hpp" // g_Log
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

	if (!json.has("version"))
		return nullptr;

	auto const &version = json.objectValue["version"].intValue;
	if (version != JSON_VERSION) {
		auto const err = std::string("Config version is") + std::to_string(version) + ", expected " + std::to_string(JSON_VERSION);
		throw std::runtime_error(err.c_str());
	}

	SmallMap<std::string /*caption*/, IFilter*> modulesDesc;

	if (!json.has("modules"))
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
					auto const err = std::string("\"modules\" unknown member: ") + prop.key;
					throw std::runtime_error(err.c_str());
				}
			}

			auto va = parseModuleConfig(moduleType, moduleConfig);
			modulesDesc[moduleCaption] = pipeline->add(moduleType.c_str(), va.get());
		}
	}

	if (json.has("connections")) {
		auto &connections = json["connections"];
		for (auto const &conn : connections.arrayValue) {
			OutputPin src(nullptr);
			InputPin dst(nullptr);
			auto first = true;
			for (auto const &prop : conn.objectValue) {
				if (first) {
					src.mod = modulesDesc[prop.key];
					if(!src.mod) {
						auto const err = std::string("Connection: source module \"" + prop.key + "\" unknown");
						throw std::runtime_error(err.c_str());
					}

					src.index = prop.value.intValue;
					first = false;
				} else {
					dst.mod = modulesDesc[prop.key];
					if (!dst.mod) {
						auto const err = std::string("Connection: destination module \"" + prop.key + "\" unknown");
						throw std::runtime_error(err.c_str());
					}

					dst.index = prop.value.intValue;
					pipeline->connect(src, dst);
				}
			}
		}
	}

	return pipeline;
}

}
