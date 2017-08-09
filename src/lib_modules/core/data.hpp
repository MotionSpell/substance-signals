#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>
#include "lib_utils/clock.hpp"

namespace Modules {

struct IMetadata;

//A generic timed data container.
class DataBase {
	public:
		DataBase() = default;
		virtual ~DataBase() {}
		virtual bool isRecyclable() const = 0;
		virtual uint8_t* data() = 0;
		virtual const uint8_t* data() const = 0;
		virtual uint64_t size() const = 0;
		virtual void resize(size_t size) = 0;

		std::shared_ptr<const IMetadata> getMetadata() const;
		void setMetadata(std::shared_ptr<const IMetadata> metadata);
		void setTime(uint64_t timeIn180k, uint64_t timescale = Clock::Rate);
		uint64_t getMediaTime() const;
		uint64_t getAbsTime(uint64_t timescale = Clock::Rate) const;

	private:
		uint64_t timeIn180k = 0, absUTCOffsetInMs = 0;
		std::shared_ptr<const IMetadata> m_metadata;
};

typedef std::shared_ptr<const DataBase> Data;

/* automatic inputs have a loose datatype */
struct DataLoose : public DataBase {};

class DataRaw : public DataBase {
	public:
		DataRaw(size_t size) : buffer(size) {}
		uint8_t* data() override;
		bool isRecyclable() const override;
		const uint8_t* data() const override;
		uint64_t size() const override;
		void resize(size_t size) override;

	private:
		std::vector<uint8_t> buffer;
};

}
