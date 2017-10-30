#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>
#include "clock.hpp"

namespace Modules {

struct IMetadata;

class IData {
public:
	IData() = default;
	virtual ~IData() {}
	virtual bool isRecyclable() const = 0;
	virtual uint8_t* data() = 0;
	virtual const uint8_t* data() const = 0;
	virtual uint64_t size() const = 0;
	virtual void resize(size_t size) = 0;
};

//A generic timed data container with metadata.
class DataBase : public IData {
public:
	DataBase(std::shared_ptr<const DataBase> data = nullptr);
	virtual ~DataBase() = default;

	std::shared_ptr<IData> getData();

	bool isRecyclable() const override;
	uint8_t* data() override;
	const uint8_t* data() const override;
	uint64_t size() const override;
	void resize(size_t size) override;

	std::shared_ptr<const IMetadata> getMetadata() const;
	void setMetadata(std::shared_ptr<const IMetadata> metadata);

	void setMediaTime(int64_t timeIn180k, uint64_t timescale = Clock::Rate);
	void setClockTime(int64_t timeIn180k, uint64_t timescale = Clock::Rate); /*should be set automatically after data is allocated*/
	int64_t getMediaTime() const;
	int64_t getClockTime(uint64_t timescale = Clock::Rate) const;
	static std::atomic<uint64_t> absUTCOffsetInMs;

private:
	int64_t mediaTimeIn180k = 0, clockTimeIn180k = 0;
	std::shared_ptr<const IMetadata> metadata;
	std::shared_ptr<IData> data_;
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
