#include <cstdio>
#include <cstdlib>
#include <iostream>
#include "optionparser/optionparser.h"
#include "lib_appcommon/options.hpp"
#include "lib_utils/format.hpp"
#include "lib_utils/tools.hpp"
#include "config.hpp"

extern const char *g_appName;

namespace {

struct Arg : public option::Arg {
	static void printError(std::string msg) {
		fprintf(stderr, "%s\n", msg.c_str());
	}

	static option::ArgStatus Unknown(const option::Option& option, bool msg) {
		if (msg) printError(format("Unknown option '%s'", option));
		return option::ARG_ILLEGAL;
	}

	static option::ArgStatus Required(const option::Option& option, bool msg) {
		if (option.arg != 0)
			return option::ARG_OK;

		if (msg) printError(format("Option '%s' requires an argument", option));
		return option::ARG_ILLEGAL;
	}

	static option::ArgStatus NonEmpty(const option::Option& option, bool msg) {
		if (option.arg != 0 && option.arg[0] != 0)
			return option::ARG_OK;

		if (msg) printError(format("Option '%s' requires a non-empty argument", option));
		return option::ARG_ILLEGAL;
	}

	static option::ArgStatus Numeric(const option::Option& option, bool msg) {
		char* endptr = 0;
		if (option.arg != 0 && strtol(option.arg, &endptr, 10)) {}
		if (endptr != option.arg && *endptr == 0)
			return option::ARG_OK;

		if (msg) printError(format("Option '%s' requires a numeric argument", option));
		return option::ARG_ILLEGAL;
	}

	static option::ArgStatus Video(const option::Option& option, bool msg) {
		unsigned w = 0, h = 0, bitrate = 0, type = 0;
		if (option.arg != 0 && sscanf(option.arg, "%ux%u:%u:%u", &w, &h, &bitrate, &type) >= 3)
			return option::ARG_OK;

		if (msg) printError(format("Option '%s' requires a video (wxh:bitrate) argument", option));
		return option::ARG_ILLEGAL;
	}
};

enum optionIndex { UNKNOWN, HELP, OPT, REQUIRED, NUMERIC, VIDEO, NONEMPTY };

void printDetectedOptions(option::Parser &parse, option::Option * const options) {
	if (parse.nonOptionsCount() >= 1 || parse.nonOptionsCount() <= 2) {
		std::cout << "URL: " << parse.nonOption(0) << std::endl;
		for (int i = 1; i < parse.nonOptionsCount(); ++i)
			std::cout << "URL: " << parse.nonOption(i) << std::endl;
	} else {
		std::cout << "Unparsed options: " << std::endl;
		for (int i = 0; i < parse.nonOptionsCount(); ++i)
			std::cout << "Unknown option: " << parse.nonOption(i) << std::endl;
		throw std::runtime_error("Parse error (1). Please check message and usage above.");
	}

	for (option::Option* opt = options[NUMERIC]; opt; opt = opt->next())
		std::cout << "Option: " << opt->name << ", value: " << atol(opt->arg) << std::endl;
	for (option::Option* opt = options[VIDEO]; opt; opt = opt->next())
		std::cout << "Option: " << opt->name << ", value: " << opt->arg << std::endl;
	for (option::Option* opt = options[OPT]; opt; opt = opt->next())
		std::cout << "Option: " << opt->name << std::endl;
	for (option::Option* opt = options[NONEMPTY]; opt; opt = opt->next())
		std::cout << "Option: " << opt->name << std::endl;
	std::cout << std::endl;
}

}

Config processArgs(int argc, char const* argv[]) {
	auto const usage0 = format("Usage: %s [options] <URL>\n\nOptions:", g_appName);
	auto const examples = format(
	        "\nExamples:\n"
	        "No transcode:\n"
	        "  %s -l -s 10000 file.mp4\n"
	        "  %s file.ts\n"
	        "  %s udp://226.0.0.1:1234\n"
	        "Transcode:\n"
	        "  %s --live --loop --seg-dur 10000 --dvr 10 --autorotate --video 320x180:50000 --video 640x360:300000 http://server.com/file.mp4\n"
	        "  %s --live -v 1280x720:1000000 webcam:video=/dev/video0:audio=/dev/audio1\n"
	        "  %s --live --working-dir workdir -v 640x360:300000 -v 1280x720:1000000 webcam:video=/dev/video0:audio=/dev/audio1\n"
	        "  %s -ilr -w tmp -t 10 -v 640x360:300000:0 udp://226.0.0.1:1234\n",
	        g_appName, g_appName, g_appName, g_appName, g_appName, g_appName, g_appName, g_appName);
	const option::Descriptor usage[] = {
		{ UNKNOWN,  0, "",  "",                   Arg::Unknown, usage0.c_str() },
		{ HELP,     0, "h", "help",               Arg::None,     "  --help,              -h             \tPrint usage and exit." },
		{ OPT,      0, "i", "loop",               Arg::None,     "  --loop,              -i             \tLoops the input indefinitely." },
		{ OPT,      0, "l", "live",               Arg::None,     "  --live,              -l             \tRun at system clock pace (otherwise runs as fast as possible)." },
		{ OPT,      0, "u", "ultra-low-latency",  Arg::None,     "  --ultra-low-latency, -u             \tLower the latency as much as possible (quality may be degraded)." },
		{ NUMERIC,  0, "s", "seg-dur",            Arg::Numeric,  "  --seg-dur,           -s             \tSet the segment duration (in ms) (default value: 2000)." },
		{ NUMERIC,  0, "t", "dvr",                Arg::Numeric,  "  --dvr,               -t             \tSet the timeshift buffer depth in segment number (default value: infinite(0))." },
		{ VIDEO,    0, "v", "video",              Arg::Video,    "  --video wxh[:b[:t]], -v wxh:b[:t]   \tSet a video resolution and optionally bitrate (enables resize and/or transcoding) and encoder type (supported 0 (software (default)), 1 (QuickSync), 2 (NVEnc)." },
		{ OPT,      0, "r", "autorotate",         Arg::None,     "  --autorotate,        -r             \tAuto-rotate if the input height is bigger than the width." },
		{ NONEMPTY, 0, "w", "working-dir",        Arg::NonEmpty, "  --working-dir,       -w             \tSet a working directory." },
		{ UNKNOWN,  0, "",  "",                   Arg::None, examples.c_str() },
		{ 0, 0, 0, 0, 0, 0 }
	};

	argc -= (argc > 0); argv += (argc > 0);
	option::Stats stats(usage, argc, argv);
	std::unique_ptr<option::Option[]> options(new option::Option[stats.options_max]);
	std::unique_ptr<option::Option[]>  buffer(new option::Option[stats.buffer_max]);
	option::Parser parse(usage, argc, argv, options.get(), buffer.get());

	if (parse.error()) {
		option::printUsage(std::cerr, usage);
		throw std::runtime_error("Parse error (2). Please check message and usage above.");
	}

	if (options[HELP] || argc == 0 || parse.nonOptionsCount() == 0) {
		option::printUsage(std::cout, usage);
		throw std::runtime_error("Please check message and usage above.");
	}

	printDetectedOptions(parse, options.get());

	auto opt = make_unique<Config>();
	opt->input = parse.nonOption(0);
	for (option::Option *o = options[OPT]; o; o = o->next()) {
		if (o->desc->shortopt == std::string("l")) {
			opt->isLive = true;
		}
		if (o->desc->shortopt == std::string("i")) {
			opt->loop = true;
		}
		if (o->desc->shortopt == std::string("u")) {
			opt->ultraLowLatency = true;
		}
		if (o->desc->shortopt == std::string("r")) {
			opt->autoRotate = true;
		}
	}
	if (options[NUMERIC].first()->desc) {
		if (options[NUMERIC].first()->desc->shortopt == std::string("s")) {
			opt->segmentDurationInMs = atoll(options[NUMERIC].first()->arg);
		} else if (options[NUMERIC].first()->desc->shortopt == std::string("t")) {
			opt->timeshiftInSegNum = atoll(options[NUMERIC].first()->arg);
		}
	}
	if (options[VIDEO].first()->desc && options[VIDEO].first()->desc->shortopt == std::string("v")) {
		unsigned w=0, h=0, type=0;
		int bitrate = 0;
		for (option::Option* o = options[VIDEO]; o; o = o->next()) {
			auto const parsed = sscanf(o->arg, "%ux%u:%u:%u", &w, &h, &bitrate, &type);
			if (parsed < 2) /*bitrate is optional*/
				throw std::runtime_error("Internal error while retrieving resolution.");
			opt->v.push_back(Video{Resolution(w, h), bitrate, (int)type});
		}
	}
	for (option::Option *o = options[NONEMPTY]; o; o = o->next()) {
		if (o->desc->shortopt == std::string("w"))
			opt->workingDir = o->arg;
	}

	return *opt;
}
