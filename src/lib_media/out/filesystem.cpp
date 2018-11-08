#include "filesystem.hpp"
#include "lib_media/common/metadata_file.hpp"
#include "lib_modules/utils/helper.hpp"
#include "lib_modules/utils/factory.hpp"

#include <map>
#include <fstream>

using namespace Modules;

namespace {

class FileSystemSink : public ModuleS {
	public:

		FileSystemSink(KHost* host, FileSystemSinkConfig cfg)
			: m_host(host),
			  m_config(cfg) {
			addInput(this);
		}

		void process(Data data) override {
			auto meta = safe_cast<const MetadataFile>(data->getMetadata());

			auto const path = m_config.directory + "/" + meta->filename;

			if(m_files.find(path) == m_files.end())
				m_files[path] = std::ofstream(path, std::ios::binary);

			auto& fp = m_files[path];
			auto contents = data->data();
			fp.write((char*)contents.ptr, contents.len);

			if(meta->EOS)
				m_files.erase(path);
		}

	private:
		KHost* const m_host;
		FileSystemSinkConfig const m_config;
		std::map<std::string, std::ofstream> m_files;
};

Modules::IModule* createObject(KHost* host, va_list va) {
	auto config = va_arg(va, FileSystemSinkConfig*);
	enforce(host, "FileSystemSink: host can't be NULL");
	enforce(config, "FileSystemSink: config can't be NULL");
	return Modules::create<FileSystemSink>(host, *config).release();
}

auto const registered = Factory::registerModule("FileSystemSink", &createObject);
}
