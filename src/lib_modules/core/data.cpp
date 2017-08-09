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
	this->timeIn180k = timescaleToClock(timeIn180k, timescale);
	if (!absUTCOffsetInMs) {
		absUTCOffsetInMs = getUTCInMs();
	}
}

uint64_t DataBase::getMediaTime() const {
	return timeIn180k;
}

uint64_t DataBase::getAbsTime(uint64_t timescale) const {
	return timescaleToClock(absUTCOffsetInMs, timescale);
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
