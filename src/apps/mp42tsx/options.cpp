#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include "optionparser/optionparser.h"
#include "options.hpp"


mp42tsXOptions processArgs(int argc, char const* argv[]) {
	(void)argc;
	(void)argv;
	mp42tsXOptions opt;
	opt.url = "av_360.mp4"; //FIXME: hardcoded
	return opt;
}
