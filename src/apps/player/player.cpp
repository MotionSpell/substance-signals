#include "pipeliner_player.hpp"
#include <iostream>

using namespace Pipelines;

namespace {
const char* processArgs(int argc, char const* argv[]) {
	if (argc != 2)
		throw std::runtime_error("usage: player <URL>");

	return argv[1];
}
}

int safeMain(int argc, char const* argv[]) {
	auto const url = processArgs(argc, argv);

	Pipeline pipeline(true, 1.0);
	declarePipeline(pipeline, url);
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
