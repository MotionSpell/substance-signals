#pragma once

struct mp42tsXOptions {
	std::string url;
	bool isLive = false;
};

mp42tsXOptions parseCommandLine(int argc, char const* argv[]);
