#include "lib_utils/tools.hpp"
#include "lib_utils/format.hpp"
#include "lib_utils/log_sink.hpp" // Info
#include "file.hpp"

#define IOSIZE (66176)

static_assert(IOSIZE % 32 == 0, "IOSIZE must be a multiple of 32");
static_assert(IOSIZE % 188 == 0, "IOSIZE must be a multiple of 188");

namespace Modules {
namespace In {

File::File(KHost* host, std::string const& fn)
	: m_host(host) {
	file = fopen(fn.c_str(), "rb");
	if (!file)
		throw error(format("Can't open file for reading: %s", fn));
	fseek(file, 0, SEEK_END);
	auto const size = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (size > IOSIZE)
		m_host->log(Info, format("File %s size is %s, will be sent by %s bytes chunks. Check the downstream modules are able to aggregate data frames.", fn, size, IOSIZE).c_str());

	m_host->activate(true);

	output = addOutput<OutputDefault>();
}

File::~File() {
	fclose(file);
}

void File::process() {
	auto out = output->getBuffer(IOSIZE);
	size_t read = fread(out->data().ptr, 1, IOSIZE, file);
	if (read == 0) {
		m_host->activate(false);
		return;
	}
	out->resize(read);
	output->post(out);
}

}
}
