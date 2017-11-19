#include "data.hpp"
#include "lib_utils/time.hpp"

namespace Modules {
std::atomic<uint64_t> DataBase::absUTCOffsetInMs(0);

DataBase::DataBase(IData * const data) {
	data_ = shptr(data, [](IData*) {});
}

DataBase::DataBase(std::shared_ptr<const DataBase> data) {
	if (data) {
		setMediaTime(data->getMediaTime());
		setClockTime(data->getClockTime());
		setMetadata(data->getMetadata());
		data_ = data->getData();
	}
}

std::shared_ptr<IData> DataBase::getData() const {
	return data_;
}

bool DataBase::isRecyclable() const {
	return data_->isRecyclable();
}

uint8_t* DataBase::data() {
	return data_->data();
}

const uint8_t* DataBase::data() const {
	return data_->data();
}

uint64_t DataBase::size() const {
	return data_->size();
}

void DataBase::resize(size_t size) {
	data_->resize(size);
}

std::shared_ptr<const IMetadata> DataBase::getMetadata() const {
	return metadata;
}

void DataBase::setMetadata(std::shared_ptr<const IMetadata> metadata) {
	this->metadata = metadata;
}

void DataBase::setMediaTime(int64_t time, uint64_t timescale) {
	mediaTimeIn180k = timescaleToClock(time, timescale);
	if (!absUTCOffsetInMs) {
		absUTCOffsetInMs = getUTC().num;
	}
}

void DataBase::setClockTime(int64_t time, uint64_t timescale) {
	clockTimeIn180k = timescaleToClock(time, timescale);
}

int64_t DataBase::getMediaTime() const {
	return mediaTimeIn180k;
}

int64_t DataBase::getClockTime(uint64_t timescale) const {
	return timescaleToClock(clockTimeIn180k, timescale);
}

DataRaw::DataRaw(size_t size) : DataBase(this), buffer(size) {
}

uint8_t* DataRaw::data() {
	return buffer.data();
}

bool DataRaw::isRecyclable() const {
	return true;
}

const uint8_t* DataRaw::data() const {
	return buffer.data();
}

uint64_t DataRaw::size() const {
	return buffer.size();
}

void DataRaw::resize(size_t size) {
	buffer.resize(size);
}

}
