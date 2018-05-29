#pragma once

struct mp42tsXOptions {
	std::string url;
};

mp42tsXOptions parseCommandLine(int argc, char const* argv[]);
