#include "graph_builder.hpp"
#include "lib_utils/log.hpp" // g_Log
#include "lib_utils/small_map.hpp"
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

std::unique_ptr<Pipeline> createPipelineFromJSON(std::string json) {
	auto pipeline = uptr(new Pipeline);

	Document d;
	d.Parse(json.c_str());
	if (d.HasParseError()) {
		std::cerr << "Error (offset " << static_cast<unsigned>(d.GetErrorOffset()) << "): " << GetParseError_En(d.GetParseError()) << "%s" << std::endl;
		throw std::runtime_error(std::string("Invalid JSON:\n") + json);
	}

	try {
		auto const &version = d["version"];
		if (version.GetUint() != JSON_VERSION) {
			auto const err = std::string("Config version is") + std::to_string(version.GetUint()) + ", expected " + std::to_string(JSON_VERSION);
			throw std::runtime_error(err.c_str());
		}

		struct ModuleDesc {
			IFilter *inst;
		};
		SmallMap<std::string /*caption*/, ModuleDesc> modulesDesc;

		{
			auto &modules = d["modules"];
			for (auto const &module : modules.GetObject()) {
				for (auto const &prop : module.value.GetObject()) {
					if (prop.name.GetString() == std::string("type")) {
						modulesDesc[module.name.GetString()] = ModuleDesc { pipeline->add(prop.value.GetString(), nullptr) };
					}
				}
			}
		}

		{
			auto &connections = d["connections"];
			for (auto const &conn : connections.GetArray()) {
				InputPin i(nullptr);
				OutputPin o(nullptr);
				bool first = true;
				assert(conn.GetObject().MemberCount() == 2);
				for (auto const &prop : conn.GetObject()) {
					if (first) {
						o.mod = modulesDesc[prop.name.GetString()].inst;
						o.index = prop.value.GetUint();
						first = false;
					} else {
						i.mod = modulesDesc[prop.name.GetString()].inst;
						i.index = prop.value.GetUint();
						pipeline->connect(o, i);
					}
				}
			}
		}
	} catch (std::runtime_error const& e) {
		auto const err = std::string("Catched error: ") + e.what();
		g_Log->log(Error, err.c_str());
		return nullptr;
	}

	return pipeline;
}

}
