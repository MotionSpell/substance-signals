#include "lib_pipeline/pipeline.hpp"
#include "lib_utils/profiler.hpp"
#include "options.hpp"
#include <csignal>

extern std::unique_ptr<const IConfig> processArgs(int argc, char const* argv[]);
extern std::unique_ptr<Pipelines::Pipeline> buildPipeline(const IConfig &config);

static Pipelines::Pipeline *g_Pipeline = nullptr;
extern const char *g_appName;
extern const char *g_version;

namespace {
#ifdef _MSC_VER
static BOOL WINAPI signalHandler(_In_ DWORD dwCtrlType) {
	switch (dwCtrlType) {
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		printf("Exit event received.\n\n");
		if (g_Pipeline) {
			g_Pipeline->exitSync();
			g_Pipeline = nullptr;
		}
		return TRUE;
	default:
		return FALSE;
	}
}
#else
static void sigTermHandler(int sig) {
	switch (sig) {
	case SIGINT:
	case SIGTERM: {
		static int numSig = 0;
		numSig++;
		if (numSig >= 3) {
			std::cerr << "Caught " << numSig-1 << " signals, hard exit." << std::endl;
			exit(3);
		} else {
			std::cerr << "Caught signal, exiting." << std::endl;
			if (g_Pipeline) {
				g_Pipeline->exitSync();
				g_Pipeline = nullptr;
			}
		}
	}
	break;
	default:
		break;
	}
}
#endif

void appInfo(int argc, char const* argv[]) {
	std::string argvs;
	for (int i = 1; i < argc; ++i) {
		argvs += " ";
		argvs += argv[i];
	}
	std::cout << format("EXECUTING: %s(%s)%s", g_appName, argv[0], argvs) << std::endl;
	std::cout << format("BUILD:     %s-%s", g_appName, g_version) << std::endl;
}
}

int safeMain(int argc, char const* argv[]) {
	Tools::Profiler profilerGlobal(g_appName);
	appInfo(argc, argv);
#ifdef _MSC_VER
	SetConsoleCtrlHandler(signalHandler, TRUE);
#else
	std::signal(SIGINT, sigTermHandler);
	std::signal(SIGTERM, sigTermHandler);
#endif

	auto config = processArgs(argc, argv);
	auto pipeline = buildPipeline(*config);
	g_Pipeline = pipeline.get();

	Tools::Profiler profilerProcessing(format("%s - processing time", g_appName));
	std::cout << g_appName << " - send SIGTERM (ctrl-c) to exit cleanly." << std::endl;
	try {
		pipeline->start();
		pipeline->waitForCompletion();
	} catch (std::exception const& e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}
	g_Pipeline = nullptr;

	return 0;
}
