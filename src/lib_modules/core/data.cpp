#include "data.hpp"
#include "clock.hpp"
#include "lib_utils/time.hpp"

namespace Modules {
std::atomic<uint64_t> DataBase::absUTCOffsetInMs(0);

std::shared_ptr<const IMetadata> DataBase::getMetadata() const {
	return m_metadata;
}

void DataBase::setMetadata(std::shared_ptr<const IMetadata> metadata) {
	m_metadata = metadata;
}

void DataBase::setTime(uint64_t timeIn180k) {
	m_timeIn180k = timeIn180k;
	if (!absUTCOffsetInMs) {
		absUTCOffsetInMs = getUTCInMs();
	}
}

void DataBase::setTime(uint64_t time, uint64_t timescale) {
	m_timeIn180k = timescaleToClock(time, timescale);
}

uint64_t DataBase::getTime() const {
	return m_timeIn180k;
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
