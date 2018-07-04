#include "helper.hpp"

namespace Modules {

MetadataCap::MetadataCap(std::shared_ptr<const IMetadata> metadata) : m_metadata(metadata) {
}

void MetadataCap::setMetadata(std::shared_ptr<const IMetadata> metadata) {
	if (!setMetadataInternal(metadata))
		throw std::runtime_error("Metadata could not be set.");
}

bool MetadataCap::updateMetadata(Data &data) {
	if (!data) {
		return false;
	} else {
		auto const &metadata = data->getMetadata();
		if (!metadata) {
			const_cast<DataBase*>(data.get())->setMetadata(m_metadata);
			return true;
		} else {
			return setMetadataInternal(metadata);
		}
	}
}

bool MetadataCap::setMetadataInternal(const std::shared_ptr<const IMetadata> &metadata) {
	if (metadata == m_metadata)
		return false;
	if (m_metadata) {
		if (metadata->getStreamType() != m_metadata->getStreamType()) {
			throw std::runtime_error(format("Metadata update: incompatible types %s for data and %s for attached", metadata->getStreamType(), m_metadata->getStreamType()));
		}
		if (*m_metadata == *metadata) {
			Log::msg(Debug, "Output: metadata not equal but comparable by value. Updating.");
			m_metadata = metadata;
		} else {
			Log::msg(Info, "Metadata update from data not supported yet: output port and data won't carry the same metadata.");
		}
		return true;
	}
	Log::msg(Debug, "Output: metadata transported by data changed. Updating.");
	m_metadata = metadata;
	return true;
}

void ActiveModule::process() {
	if(getNumInputs() > 0)
		getInput(0)->pop();

	while (!mustExit() && work()) {}
}

bool ActiveModule::mustExit() {
	Data data;
	return getNumInputs() && getInput(0)->tryPop(data);
}

}
