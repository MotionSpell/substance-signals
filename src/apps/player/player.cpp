#include "pipeliner_player.hpp"
#include "config.hpp"
#include "lib_appcommon/options.hpp"
#include <iostream>
#include <chrono>
#include <thread>

using namespace std;
using namespace Pipelines;

namespace {
Config parseCommandLine(int argc, char const* argv[]) {

	Config cfg;

	CmdLineOptions opt;
	opt.addFlag("l", "lowlatency", &cfg.lowLatency, "Use low latency");
	opt.add("g", "loglevel", &cfg.logLevel, "Log level");
	opt.add("a", "stop-after", &cfg.stopAfterMs, "Stop after X ms");
	opt.add("n", "no-renderer", &cfg.noRenderer, "Stop after X ms");

	auto files = opt.parse(argc, argv);
	if (files.size() != 1) {
		std::cout << "Usage: player <URL>" << std::endl;
		opt.printHelp();
		throw std::runtime_error("invalid command line");
	}

	cfg.url = files[0];

	return cfg;
}
}

int safeMain(int argc, char const* argv[]) {
	auto const cfg = parseCommandLine(argc, argv);

	setGlobalLogLevel((Level)cfg.logLevel);

	Pipeline pipeline(cfg.lowLatency);
	declarePipeline(cfg, pipeline, cfg.url.c_str());
	pipeline.start();

	if(cfg.stopAfterMs >= 0)
		this_thread::sleep_for(chrono::milliseconds(cfg.stopAfterMs));
	else
		pipeline.waitForEndOfStream();

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
