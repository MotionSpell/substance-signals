#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include "lib_appcommon/options.hpp"
#include "options.hpp"


mp42tsXOptions parseCommandLine(int argc, char const* argv[]) {
	mp42tsXOptions cfg;

	CmdLineOptions opt;

	auto files = opt.parse(argc, argv);
	if (files.size() != 1) {
		std::cout << "Usage: mp42tsx <input.mp4>" << std::endl;
		opt.printHelp(std::cout);
		throw std::runtime_error("invalid command line");
	}

	cfg.url = files[0];

	return cfg;
}
