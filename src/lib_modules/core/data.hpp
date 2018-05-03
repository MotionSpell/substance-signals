#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>
#include "clock.hpp"

namespace Modules {

struct IMetadata;

struct IData {
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
		virtual ~DataBase() = default;

		std::shared_ptr<const IMetadata> getMetadata() const;
		void setMetadata(std::shared_ptr<const IMetadata> metadata);

		void setMediaTime(int64_t timeIn180k, uint64_t timescale = IClock::Rate);
		void setClockTime(int64_t timeIn180k, uint64_t timescale = IClock::Rate); /*should be set automatically after data is allocated*/
		int64_t getMediaTime() const;
		int64_t getClockTime(uint64_t timescale = IClock::Rate) const;
		static std::atomic<uint64_t> absUTCOffsetInMs;

	private:
		int64_t mediaTimeIn180k = 0, clockTimeIn180k = 0;
		std::shared_ptr<const IMetadata> metadata;
};

class DataBaseRef : public DataBase {
	public:
		DataBaseRef(std::shared_ptr<const DataBase> data);
		std::shared_ptr<const DataBase> getData() const;

		bool isRecyclable() const override;
		uint8_t* data() override;
		const uint8_t* data() const override;
		uint64_t size() const override;
		void resize(size_t size) override;

	private:
		std::shared_ptr<const DataBase> dataRef;
};

/* automatic inputs have a loose datatype */
struct DataLoose : public DataBase {};

class DataRaw : public DataBase {
	public:
		DataRaw(size_t size);
		uint8_t* data() override;
		bool isRecyclable() const override;
		const uint8_t* data() const override;
		uint64_t size() const override;
		void resize(size_t size) override;

	private:
		std::vector<uint8_t> buffer;
};

typedef std::shared_ptr<const DataBase> Data;

}

template<class T>
std::shared_ptr<T> safe_cast(std::shared_ptr<const Modules::DataBase> p) {
	if (!p)
		return nullptr;
	auto r = std::dynamic_pointer_cast<T>(p);
	if (r) {
		return r;
	} else {
		auto s = std::dynamic_pointer_cast<const Modules::DataBaseRef>(p);
		if (s) {
			auto t = std::dynamic_pointer_cast<T>(s->getData());
			if (t) {
				return t;
			} else {
				auto u = std::dynamic_pointer_cast<const Modules::DataBase>(s->getData());
				if (u)
					return safe_cast<T>(u);
			}
		}
	}
	throw std::runtime_error(format("dynamic cast error: could not convert from Modules::Data to %s", typeid(T).name()));
}
