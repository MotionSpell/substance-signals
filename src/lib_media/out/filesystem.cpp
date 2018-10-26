#include "filesystem.hpp"
#include "lib_utils/tools.hpp"
#include "lib_media/common/metadata_file.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_modules/utils/factory.hpp"

#include <fstream>

using namespace Modules;

namespace {

class FileSystemSink : public ModuleS {
	public:

		FileSystemSink(IModuleHost* host, FileSystemSinkConfig cfg)
			: m_host(host),
			  m_config(cfg) {
			addInput(this);
		}

		void flush() override {
			if(m_contents.empty())
				return;
			std::ofstream fp(m_config.directory + "/" + m_currentFilename);
			fp.write((char*)m_contents.data(), m_contents.size());
			m_contents.clear();
		}

		void process(Data data) override {
			auto meta = safe_cast<const MetadataFile>(data->getMetadata());

			m_currentFilename = meta->filename;
			if (data->data().len != 0)
				m_contents.insert(m_contents.end(), data->data().ptr, data->data().ptr + data->data().len);

			if (meta->EOS)
				flush();
		}

	private:
		IModuleHost* const m_host;
		FileSystemSinkConfig const m_config;
		std::vector<uint8_t> m_contents;
		std::string m_currentFilename;
};

Modules::IModule* createObject(IModuleHost* host, va_list va) {
	auto config = va_arg(va, FileSystemSinkConfig*);
	enforce(host, "FileSystemSink: host can't be NULL");
	enforce(config, "FileSystemSink: config can't be NULL");
	return Modules::create<FileSystemSink>(host, *config).release();
}

auto const registered = Factory::registerModule("FileSystemSink", &createObject);
}
