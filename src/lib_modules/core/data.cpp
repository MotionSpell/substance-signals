#include "data.hpp"
#include "lib_utils/time.hpp"

namespace Modules {

std::shared_ptr<const IMetadata> DataBase::getMetadata() const {
	return m_metadata;
}

void DataBase::setMetadata(std::shared_ptr<const IMetadata> metadata) {
	m_metadata = metadata;
}

void DataBase::setMediaTime(uint64_t timeIn180k, uint64_t timescale) {
	mediaTimeIn180k = timescaleToClock(timeIn180k, timescale);
}

void DataBase::setClockTime(uint64_t timeIn180k, uint64_t timescale) {
	clockTimeIn180k = timescaleToClock(timeIn180k, timescale);
}

uint64_t DataBase::getMediaTime() const {
	return mediaTimeIn180k;
}

uint64_t DataBase::getClockTime(uint64_t timescale) const {
	return timescaleToClock(clockTimeIn180k, timescale);
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
