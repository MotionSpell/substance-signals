#include "lib_utils/profiler.hpp"
#include "pipeliner.hpp"
#include <csignal>
#ifdef _WIN32
#include <direct.h> //chdir
#else
#include <unistd.h>
#endif
#include <gpac/tools.h> //gf_mkdir

using namespace Pipelines;

Pipeline *g_Pipeline = nullptr;
extern const char *g_appName;

#ifdef _MSC_VER
BOOL WINAPI signalHandler(_In_ DWORD dwCtrlType) {
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
void sigTermHandler(int sig) {
	if (sig == SIGTERM) {
		std::cerr << "Caught signal(SIGTERM), exiting." << std::endl;
		if (g_Pipeline) {
			g_Pipeline->exitSync();
			g_Pipeline = nullptr;
		}
	}
}
#endif

int safeMain(int argc, char const* argv[], const FormatFlags formats) {
#ifdef _MSC_VER
	SetConsoleCtrlHandler(signalHandler, TRUE);
#else
	std::signal(SIGTERM, sigTermHandler);
#endif

	AppOptions opt = processArgs(argc, argv);
	if (chdir(opt.workingDir.c_str()) < 0 && (gf_mkdir((char*)opt.workingDir.c_str()) || chdir(opt.workingDir.c_str()) < 0))
		throw std::runtime_error(format("%s - couldn't change dir to %s: please check the directory exists and you have sufficient rights", g_appName, opt.workingDir));

	Tools::Profiler profilerGlobal(g_appName);

	auto pipeline = uptr(new Pipeline(opt.ultraLowLatency, opt.isLive ? 1.0 : 0.0, opt.ultraLowLatency ? Pipeline::Mono : Pipeline::OnePerModule));
	declarePipeline(*pipeline, opt, formats);
	g_Pipeline = pipeline.get();

	Tools::Profiler profilerProcessing(format("%s - processing time", g_appName), std::cerr);
	std::cerr << g_appName << " - close window or ctrl-c to exit cleanly." << std::endl;
	try {
		pipeline->start();
		pipeline->waitForCompletion();
	} catch (std::exception const& e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}
	g_Pipeline = nullptr;

	return 0;
}
