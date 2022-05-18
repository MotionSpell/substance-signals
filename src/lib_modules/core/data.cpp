#include "database.hpp"
#include "raw_buffer.hpp"
#include <cstring> // memcpy
#include <stdexcept> //runtime_error

namespace Modules {

std::shared_ptr<const IMetadata> DataBase::getMetadata() const {
	return metadata;
}

void DataBase::setMetadata(std::shared_ptr<const IMetadata> metadata) {
	this->metadata = metadata;
}

SpanC DataBase::getAttribute(int typeId) const {
	auto first = attributeOffset.find(typeId);
	if(first == attributeOffset.end())
		throw std::runtime_error("Attribute not found");
	return {attributes.data() + (*first).value, 0};
}

void DataBase::setAttribute(int typeId, SpanC data) {
	if(!data.ptr)
		throw std::runtime_error("Can't set a NULL attribute");

	{
		auto first = attributeOffset.find(typeId);
		if(first != attributeOffset.end()) {
			// HACK for PresentationTime. Remove this when the clone code is fixed.
			if(typeId == 0x35A12022) {
				memcpy(attributes.data() + (*first).value, data.ptr, data.len);
				return;
			}

			throw std::runtime_error("Attribute is already set");
		}
	}

	auto offset = attributes.size();
	attributeOffset[typeId] = offset;
	attributes.resize(offset + data.len);
	memcpy(attributes.data() + offset, data.ptr, data.len);
}

void DataBase::copyAttributes(DataBase const& from) {
	attributeOffset = from.attributeOffset;
	attributes = from.attributes;
}

std::shared_ptr<DataBase> clone(std::shared_ptr<const DataBase> data) {
	auto clone = std::make_shared<DataBaseRef>(data);
	clone->copyAttributes(*data);
	return clone;
}

DataBaseRef::DataBaseRef(std::shared_ptr<const DataBase> data) {
	if (data) {
		// don't copy attributes as they can't be overwritten
		setMetadata(data->getMetadata());
		auto ref = std::dynamic_pointer_cast<const DataBaseRef>(data);
		if (ref) {
			dataRef = ref->getData();
		} else {
			dataRef = data;
		}
		buffer = dataRef->buffer;
	}
}

std::shared_ptr<const DataBase> DataBaseRef::getData() const {
	return dataRef;
}

DataRaw::DataRaw(size_t size) {
	buffer = std::make_shared<RawBuffer>(size);
}

DataRawResizable::DataRawResizable(size_t size) : DataRaw(size) {
}

void DataRawResizable::resize(size_t size) {
	std::dynamic_pointer_cast<RawBuffer>(buffer)->resize(size);
}

}
