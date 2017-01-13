#include "gpac.hpp"
#include "lib_gpacpp/gpacpp.hpp"

namespace Modules {

DataRawGPAC::~DataRawGPAC() {
	gf_free((void*)buffer);
}

}
