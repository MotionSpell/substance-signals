#include "lib_utils/profiler.hpp"
#include <csignal>
#include <iostream> // cerr

// user-provided
extern void safeMain(int argc, const char* argv[]);
extern void safeStop();

extern const char *g_appName;
extern const char *g_version;

static void onInterruption() {
	static int numSig = 0;
	numSig++;
	if (numSig >= 3) {
		std::cerr << "Caught " << numSig << " signals, hard exit." << std::endl;
		exit(3);
	} else {
		std::cerr << "Caught signal, exiting." << std::endl;
		safeStop();
	}
}

#ifdef _MSC_VER
#include <Windows.h>
static BOOL WINAPI signalHandler(_In_ DWORD dwCtrlType) {
	switch (dwCtrlType) {
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		onInterruption();
		return TRUE;
	default:
		return FALSE;
	}
}

static void installSignalHandler() {
	SetConsoleCtrlHandler(signalHandler, TRUE);
}

#else
static void sigTermHandler(int sig) {
	switch (sig) {
	case SIGINT:
	case SIGTERM: {
		onInterruption();
	}
	break;
	default:
		break;
	}
}

static void installSignalHandler() {
	std::signal(SIGINT, sigTermHandler);
	std::signal(SIGTERM, sigTermHandler);
}

#endif

int main(int argc, char const* argv[]) {
	try {
		Tools::Profiler profilerGlobal(g_appName);
		std::cout << "BUILD:     " << g_appName << "-" << g_version << std::endl;

		installSignalHandler();
		safeMain(argc, argv);

		return 0;
	} catch (std::exception const& e) {
		std::cerr << "[" << g_appName << "] " << "Error: " << e.what() << std::endl;
		return 1;
	}
}

