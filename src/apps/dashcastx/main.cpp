#include <memory>
#include "lib_appcommon/options.hpp"
#include "lib_utils/profiler.hpp"
#include "lib_pipeline/pipeline.hpp"

const char *g_appName = "dashcastx";

extern std::unique_ptr<const IConfig> processArgs(int argc, char const* argv[]);
extern std::unique_ptr<Pipelines::Pipeline> buildPipeline(const IConfig &config);
static Pipelines::Pipeline *g_Pipeline = nullptr;

void safeMain(int argc, const char* argv[]) {
	auto config = processArgs(argc, argv);
	auto pipeline = buildPipeline(*config);
	g_Pipeline = pipeline.get();

	Tools::Profiler profilerProcessing(format("%s - processing time", g_appName));
	pipeline->start();
	pipeline->waitForEndOfStream();
}

void safeStop() {
	if (g_Pipeline) {
		g_Pipeline->exitSync();
		g_Pipeline = nullptr;
	}
}

