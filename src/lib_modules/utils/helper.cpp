#include "helper.hpp"
#include "helper_dyn.hpp"
#include "helper_input.hpp"
#include "lib_utils/log.hpp"
#include "lib_utils/format.hpp"

namespace Modules {

KInput* Module::addInput(IProcessor* p) {
	inputs.push_back(make_unique<Input>(p));
	return inputs.back().get();
}

MetadataCap::MetadataCap(Metadata metadata) : m_metadata(metadata) {
}

void MetadataCap::setMetadata(Metadata metadata) {
	if (!setMetadataInternal(metadata))
		throw std::runtime_error("Metadata could not be set.");
}

bool MetadataCap::updateMetadata(Data &data) {
	if (!data)
		return false;

	auto const &metadata = data->getMetadata();
	if (!metadata) {
		const_cast<DataBase*>(data.get())->setMetadata(m_metadata);
		return true;
	} else {
		return setMetadataInternal(metadata);
	}
}

bool MetadataCap::setMetadataInternal(Metadata metadata) {
	if (metadata == m_metadata)
		return false;
	if (m_metadata) {
		if (metadata->type != m_metadata->type) {
			throw std::runtime_error(format("Metadata update: incompatible types %s for data and %s for attached", metadata->type, m_metadata->type));
		}
		if (*m_metadata == *metadata) {
			g_Log->log(Debug, "Output: metadata not equal but comparable by value. Updating.");
			m_metadata = metadata;
		} else {
			g_Log->log(Info, "Metadata update from data not supported yet: output port and data won't carry the same metadata.");
		}
		return true;
	}
	g_Log->log(Debug, "Output: metadata transported by data changed. Updating.");
	m_metadata = metadata;
	return true;
}

void ActiveModule::process() {
	while (work()) {}
}

void NullHostType::log(int level, char const* msg) {
	if(0)
		printf("[%d] %s\n", level, msg);
}

}
