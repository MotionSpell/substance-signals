#include "lib_appcommon/options.hpp"
#include "lib_pipeline/pipeline.hpp"
#include "lib_media/out/UdpOutput/udp_output.hpp"
#include "lib_media/in/file.hpp"

using namespace std;
using namespace Modules;
using namespace Pipelines;

struct Config {
	UdpOutputConfig udpConfig;
	string path;
	bool help = false;
};

Config parseCommandLine(int argc, char const* argv[]) {
	Config cfg;

	int bitrate = 50 * 1000 * 1000;

	CmdLineOptions opt;
	opt.addFlag("h", "help", &cfg.help, "Print usage and exit.");
	opt.add("b", "bitrate", &bitrate, "Set sending bitrate (default: 50Mbps)");

	auto files = opt.parse(argc, argv);

	if(cfg.help) {
		printf("Usage: %s [options] <input.ts> <dst IP address>:<dst UDP port>\nOptions:\n", argv[0]);
		opt.printHelp();
		return cfg;
	}

	if (files.size() != 2)
		throw runtime_error("invalid command line, use --help");

	cfg.path = files[0];

	if(sscanf(files[1].c_str(), "%d.%d.%d.%d:%d",
	        &cfg.udpConfig.ipAddr[0],
	        &cfg.udpConfig.ipAddr[1],
	        &cfg.udpConfig.ipAddr[2],
	        &cfg.udpConfig.ipAddr[3],
	        &cfg.udpConfig.port) != 5)
		throw runtime_error("invalid destination address format");

	cfg.udpConfig.bitrate = bitrate;

	return cfg;
}

int safeMain(int argc, char const* argv[]) {
	auto const cfg = parseCommandLine(argc, argv);
	if(cfg.help)
		return 0;

	Pipeline pipeline;

	auto file = pipeline.addModule<In::File>(cfg.path, 7 * 188);
	auto sender = pipeline.add("UdpOutput", &cfg.udpConfig);
	pipeline.connect(file, sender);
	pipeline.start();
	pipeline.waitForEndOfStream();

	return 0;
}

int main(int argc, char const* argv[]) {
	try {
		return safeMain(argc, argv);
	} catch(exception const& e) {
		fprintf(stderr, "Error: %s\n", e.what());
		return 1;
	}
}
