#include "lib_utils/tools.hpp"
#include "lib_utils/format.hpp"
#include "file.hpp"

namespace Modules {
namespace Out {

File::File(IModuleHost* host, std::string const& path)
	:  m_host(host) {
	file = fopen(path.c_str(), "wb");
	if (!file)
		throw error(format("Can't open file for writing: %s", path));

	addInput(this);
}

File::~File() {
	fclose(file);
}

void File::process(Data data_) {
	auto data = safe_cast<const DataBase>(data_);
	fwrite(data->data().ptr, 1, data->data().len, file);
}

}
}
