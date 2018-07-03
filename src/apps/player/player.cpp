#include "pipeliner_player.hpp"
#include "lib_appcommon/options.hpp"
#include <iostream>
#include <chrono>
#include <thread>

using namespace std;
using namespace Pipelines;

struct Config {
	std::string url;
	double speed = 1.0;
	bool lowLatency = false;
	int logLevel = 1;
	int stopAfterMs = -1; // by default, wait until the end of stream
};

namespace {
Config parseCommandLine(int argc, char const* argv[]) {

	Config cfg;

	CmdLineOptions opt;
	opt.addFlag("l", "lowlatency", &cfg.lowLatency, "Use low latency");
	opt.add("s", "speed", &cfg.speed, "Speed ratio");
	opt.add("g", "loglevel", &cfg.logLevel, "Log level");
	opt.add("a", "stop-after", &cfg.stopAfterMs, "Stop after X ms");

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

	Log::setLevel((Level)cfg.logLevel);

	Pipeline pipeline(cfg.lowLatency, cfg.speed);
	declarePipeline(pipeline, cfg.url.c_str());
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
