#include "database.hpp"
#include <cstring> // memcpy

namespace Modules {

std::shared_ptr<const IMetadata> DataBase::getMetadata() const {
	return metadata;
}

void DataBase::setMetadata(std::shared_ptr<const IMetadata> metadata) {
	this->metadata = metadata;
}

void DataBase::setMediaTime(int64_t time, uint64_t timescale) {
	mediaTimeIn180k = timescaleToClock(time, timescale);
}

int64_t DataBase::getMediaTime() const {
	return mediaTimeIn180k;
}

SpanC DataBase::getAttribute(int typeId) const {
	auto first = attributeOffset.find(typeId);
	if(first == attributeOffset.end())
		throw std::runtime_error("Attribute not found");
	return {attributes.data() + *first, 0};
}

void DataBase::setAttribute(int typeId, SpanC data) {
	if(!data.ptr)
		throw std::runtime_error("Can't set a NULL attribute");

	{
		auto first = attributeOffset.find(typeId);
		if(first != attributeOffset.end())
			throw std::runtime_error("Attribute is already set");
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

DataBaseRef::DataBaseRef(std::shared_ptr<const DataBase> data) {
	if (data) {
		setMediaTime(data->getMediaTime());
		setMetadata(data->getMetadata());
		auto ref = std::dynamic_pointer_cast<const DataBaseRef>(data);
		if (ref) {
			dataRef = ref->getData();
		} else {
			dataRef = data;
		}
	}
}

std::shared_ptr<const DataBase> DataBaseRef::getData() const {
	return dataRef;
}

namespace {
struct RawBuffer : IBuffer {
	RawBuffer(size_t size) : memoryBlock(size) {}
	std::vector<uint8_t> memoryBlock;

	Span data() {
		return Span { memoryBlock.data(), memoryBlock.size() };
	}

	SpanC data() const {
		return SpanC { memoryBlock.data(), memoryBlock.size() };
	}

	void resize(size_t size) {
		memoryBlock.resize(size);
	}
};
}

DataRaw::DataRaw(size_t size) : buffer(std::make_shared<RawBuffer>(size)) {
}

}

void throw_dynamic_cast_error(const char* typeName) {
	throw std::runtime_error("dynamic cast error: could not convert from Modules::Data to " + std::string(typeName));
}

