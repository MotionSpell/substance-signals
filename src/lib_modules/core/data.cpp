#include "database.hpp"
#include "data_utc.hpp"
#include "lib_utils/time.hpp"

namespace Modules {
std::atomic<uint64_t> absUTCOffsetInMs(0);

std::shared_ptr<const IMetadata> DataBase::getMetadata() const {
	return metadata;
}

void DataBase::setMetadata(std::shared_ptr<const IMetadata> metadata) {
	this->metadata = metadata;
}

void DataBase::setMediaTime(int64_t time, uint64_t timescale) {
	mediaTimeIn180k = timescaleToClock(time, timescale);
	if (!absUTCOffsetInMs) {
		absUTCOffsetInMs = int64_t(getUTC() * 1000);
	}
}

int64_t DataBase::getMediaTime() const {
	return mediaTimeIn180k;
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

bool DataBaseRef::isRecyclable() const {
	return dataRef->isRecyclable();
}

Span DataBaseRef::data() {
	throw std::runtime_error("DataBaseRef::data(): non-const operations not allowed. Aborting.");
}

SpanC DataBaseRef::data() const {
	return dataRef->data();
}

void DataBaseRef::resize(size_t /*size*/) {
	throw std::runtime_error("DataBaseRef::resize(): non-const operations not allowed. Aborting.");
}

DataRaw::DataRaw(size_t size) : buffer(size) {
}

Span DataRaw::data() {
	return Span { buffer.data(), buffer.size() };
}

SpanC DataRaw::data() const {
	return SpanC { buffer.data(), buffer.size() };
}

bool DataRaw::isRecyclable() const {
	return true;
}

void DataRaw::resize(size_t size) {
	buffer.resize(size);
}

}
