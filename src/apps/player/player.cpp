#include "pipeliner_player.hpp"
#include "lib_appcommon/options.hpp"
#include <iostream>

using namespace Pipelines;

struct Config {
	std::string url;
	bool lowLatency = false;
};

namespace {
Config parseCommandLine(int argc, char const* argv[]) {

	Config cfg;

	CmdLineOptions opt;
	opt.addFlag("l", "lowlatency", &cfg.lowLatency, "Use low latency");

	auto files = opt.parse(argc, argv);
	if (files.size() != 1) {
		std::cout << "Usage: player <URL>" << std::endl;
		opt.printHelp(std::cout);
		throw std::runtime_error("invalid command line");
	}

	cfg.url = files[0];

	return cfg;
}
}

int safeMain(int argc, char const* argv[]) {
	auto const cfg = parseCommandLine(argc, argv);

	Pipeline pipeline(cfg.lowLatency, 1.0);
	declarePipeline(pipeline, cfg.url.c_str());
	pipeline.start();
	pipeline.waitForCompletion();

	return 0;
}

int main(int argc, char const* argv[]) {
	try {
		return safeMain(argc, argv);
	} catch(std::exception const& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
}
