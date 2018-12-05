#pragma once

#include "i_filter.hpp"
#include "lib_utils/log.hpp"
#include "lib_utils/format.hpp"
#include "lib_modules/modules.hpp"
#include <vector>
#include <memory>
#include <string>

namespace Pipelines {

struct IStatsRegistry;
struct Graph;

using CreationFunc = std::function<std::shared_ptr<Modules::IModule>(Modules::KHost*)>;

// A set of interconnected processing filters.
class Pipeline : public IPipelineNotifier {
	public:
		template <typename InstanceType, int NumBlocks = 0, typename ...Args>
		IFilter * addModule(Args&&... args) {

			auto createModule = [&](Modules::KHost* host) {
				return Modules::createModule<InstanceType>(
				        getNumBlocks(NumBlocks),
				        host,
				        std::forward<Args>(args)...);
			};

			auto name = format("%s", modules.size());
			return addModuleInternal(name, createModule);
		}

		IFilter * add(char const* name, const void* va);

		/* @isLowLatency Controls the default number of buffers.
			@threading    Controls the threading. */
		Pipeline(LogSink* log = nullptr, bool isLowLatency = false, Threading threading = Threading::OnePerModule);
		virtual ~Pipeline();

		// Remove a module from a pipeline.
		// This is only possible when the module is disconnected and flush()ed
		// (which is the caller responsibility - FIXME)
		void removeModule(IFilter * module);
		void connect   (OutputPin out, InputPin in, bool inputAcceptMultipleConnections = false);
		void disconnect(IFilter * prev, int outputIdx, IFilter * next, int inputIdx);

		std::string dump() const; // dump pipeline using DOT Language

		void start();
		void waitForEndOfStream();
		void exitSync(); /*ask for all sources to finish*/

	private:
		IFilter * addModuleInternal(std::string name, CreationFunc createModule);
		void computeTopology();
		void endOfStream();
		void exception(std::exception_ptr eptr);

		/*FIXME: the block below won't be necessary once we inject correctly*/
		int getNumBlocks(int numBlock) const {
			return numBlock ? numBlock : allocatorNumBlocks;
		}

		std::unique_ptr<IStatsRegistry> statsMem;
		std::vector<std::unique_ptr<IFilter>> modules;
		std::unique_ptr<Graph> graph;
		LogSink* const m_log;
		const int allocatorNumBlocks;
		const Threading threading;

		std::mutex remainingNotificationsMutex;
		std::condition_variable condition;
		size_t notifications = 0, remainingNotifications = 0;
		std::exception_ptr eptr;
};

}
