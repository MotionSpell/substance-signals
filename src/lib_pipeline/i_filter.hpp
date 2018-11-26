#pragma once

#include "lib_modules/modules.hpp"
#include <memory>

namespace Pipelines {

enum class Threading {
	Mono              = 1,
	OnePerModule      = 2,
};

// interconnected pipeline elements, as the application sees them.
struct IFilter {
	virtual ~IFilter() {};
	virtual int getNumInputs() const = 0;
	virtual int getNumOutputs() const = 0;
	virtual Modules::Metadata getOutputMetadata(int i) = 0;
};

struct InputPin {
	InputPin(IFilter* m, int idx=0) : mod(m), index(idx) {};
	IFilter* mod;
	int index = 0;
};

struct OutputPin {
	OutputPin(IFilter* m, int idx=0) : mod(m), index(idx) {};
	IFilter* mod;
	int index = 0;
};

inline InputPin GetInputPin(IFilter* mod, int index=0) {
	return InputPin { mod, index };
}

inline OutputPin GetOutputPin(IFilter* mod, int index=0) {
	return OutputPin { mod, index };
}

struct IPipelineNotifier {
	virtual void endOfStream() = 0;
	virtual void exception(std::exception_ptr eptr) = 0;
};

}
