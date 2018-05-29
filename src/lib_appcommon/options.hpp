#pragma once

#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <sstream>

typedef std::queue<std::string> ArgQueue;

static inline std::string safePop(ArgQueue& args) {
	if(args.empty())
		throw std::runtime_error("unexpected end of command line");
	auto val = args.front();
	args.pop();
	return val;
}

static inline void parseValue(int& var, ArgQueue& args) {
	std::stringstream ss(safePop(args));
	ss >> var;
}

static inline void parseValue(bool& var, ArgQueue&) {
	var = true;
}

static inline void parseValue(std::string& var, ArgQueue& args) {
	var = safePop(args);
}

static inline void parseValue(std::vector<std::string>& var, ArgQueue& args) {
	var.clear();
	while(!args.empty() && args.front()[0] != '-') {
		var.push_back(safePop(args));
	}
}

struct CmdLineOptions {
		void addFlag(std::string shortName, std::string longName, bool* pVar, std::string desc="") {
			add(shortName, longName, pVar, desc);
		}

		template<typename T>
		void add(std::string shortName, std::string longName, T* pVar, std::string desc="") {
			auto opt = std::make_unique<TypedOption<T>>();
			opt->pVar = pVar;
			opt->shortName = "-" + shortName;
			opt->longName = "--" + longName;
			opt->desc = desc;
			m_Options.push_back(std::move(opt));
		}

		std::vector<std::string> parse(int argc, const char* argv[]);
		void printHelp(std::ostream& out);

	private:
		struct AbstractOption {
			std::string shortName, longName;
			std::string desc;
			virtual void parse(ArgQueue& args) = 0;
		};

		std::vector<std::unique_ptr<AbstractOption>> m_Options;

		template<typename T>
		struct TypedOption : AbstractOption {
			T* pVar;
			void parse(ArgQueue& args) {
				parseValue(*pVar, args);
			}
		};
};

///////////////////////////////////////////////////////////////////////////////
// Deprecated stuff: no config struct can fit all applications
// (e.g no app from this repo use 'astOffset')

#include "lib_utils/resolution.hpp"

struct Video { //FIXME: this can be factorized with other params
	Video(Resolution res, unsigned bitrate, unsigned type)
		: res(res), bitrate(bitrate), type(type) {}
	Resolution res;
	unsigned bitrate;
	unsigned type;
};

struct IConfig {
	virtual ~IConfig() {}
};

struct AppOptions : IConfig {
	std::string input, workingDir = ".", postCommand, id;
	std::vector<std::string> outputs;
	std::vector<Video> v;
	uint64_t seekTimeInMs = 0, segmentDurationInMs = 2000, timeshiftInSegNum = 0, minUpdatePeriodInMs = 0;
	uint32_t minBufferTimeInMs = 0;
	int64_t astOffset = 0;
	bool isLive = false, loop = false, ultraLowLatency = false, autoRotate = false, watermark = true;
};

std::unique_ptr<const IConfig> processArgs(int argc, char const* argv[]);
