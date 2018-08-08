#include "gpac.hpp"
#include "gpacpp.hpp"

namespace Modules {

DataRawGPAC::~DataRawGPAC() {
	gf_free((void*)buffer);
}

}
