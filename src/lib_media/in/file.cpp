#include "lib_utils/tools.hpp"
#include "file.hpp"

#define IOSIZE (64*1024)

namespace Modules {
namespace In {

File::File(IModuleHost* host, std::string const& fn)
	: m_host(host) {
	file = fopen(fn.c_str(), "rb");
	if (!file)
		throw error(format("Can't open file for reading: %s", fn));
	fseek(file, 0, SEEK_END);
	auto const size = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (size > IOSIZE)
		m_host->log(Info, format("File %s size is %s, will be sent by %s bytes chunks. Check the downstream modules are able to agregate data frames.", fn, size, IOSIZE).c_str());

	output = addOutput<OutputDefault>();
}

File::~File() {
	fclose(file);
}

bool File::work() {
	auto out = output->getBuffer(IOSIZE);
	size_t read = fread(out->data().ptr, 1, IOSIZE, file);
	if (read == 0) {
		return false;
	}
	if (read < IOSIZE) {
		out->resize(read);
	}
	output->emit(out);
	return true;
}

}
}
