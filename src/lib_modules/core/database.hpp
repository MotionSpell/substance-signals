#pragma once

#include "buffer.hpp"
#include <cstring> //memcpy
#include <memory>
#include <vector>
#include <map>
#include "lib_utils/clock.hpp"

namespace Modules {

struct IMetadata;

//A generic timed data container with metadata.
class DataBase {
	public:
		virtual ~DataBase() = default;

		std::shared_ptr<const IMetadata> getMetadata() const;
		void setMetadata(std::shared_ptr<const IMetadata> metadata);

		SpanC getAttribute(int typeId) const;
		void setAttribute(int typeId, SpanC data);
		void copyAttributes(DataBase const& from);

		template<typename Type>
		Type get() const {
			auto data = getAttribute(Type::TypeId);
			Type r;
			memcpy(&r, data.ptr, sizeof r);
			return r;
		}

		template<typename Type>
		void set(const Type& attribute) {
			static_assert(std::is_pod<Type>::value, "Type must be POD");
			setAttribute(Type::TypeId, {(const uint8_t*)&attribute, sizeof attribute});
		}

		void setMediaTime(int64_t timeIn180k, uint64_t timescale = IClock::Rate);
		int64_t getMediaTime() const;

		virtual const IBuffer* getBuffer() const = 0;
		virtual IBuffer* getBuffer() = 0;

		SpanC data() const {
			return getBuffer()->data();
		}

		Span data() {
			return getBuffer()->data();
		}

	private:
		int64_t mediaTimeIn180k = 0;
		std::shared_ptr<const IMetadata> metadata;
		std::vector<uint8_t> attributes;
		std::map<int, int> attributeOffset;
};

class DataBaseRef : public DataBase {
	public:
		DataBaseRef(std::shared_ptr<const DataBase> data);
		std::shared_ptr<const DataBase> getData() const;

		// DataBase
		const IBuffer* getBuffer() const override {
			return dataRef->getBuffer();
		}

		IBuffer* getBuffer() override {
			return (IBuffer*)dataRef->getBuffer();
		}

	private:
		std::shared_ptr<const DataBase> dataRef;
};

class DataRaw : public DataBase {
	public:
		DataRaw(size_t size);

		// DataBase
		const IBuffer* getBuffer() const override {
			return buffer.get();
		}

		IBuffer* getBuffer() override {
			return buffer.get();
		}

	private:
		std::shared_ptr<IBuffer> buffer;
};

using Data = std::shared_ptr<const DataBase>;
using Metadata = std::shared_ptr<const IMetadata>;

inline bool isDeclaration(Data data) {
	auto refData = std::dynamic_pointer_cast<const DataBaseRef>(data);
	return refData && !refData->getData();
}

}

[[noreturn]] void throw_dynamic_cast_error(const char* typeName);
template<class T>
std::shared_ptr<T> safe_cast(std::shared_ptr<const Modules::DataBase> p) {
	if (!p)
		return nullptr;
	if (auto r = std::dynamic_pointer_cast<T>(p))
		return r;

	if (auto ref = std::dynamic_pointer_cast<const Modules::DataBaseRef>(p)) {
		if (auto r = std::dynamic_pointer_cast<T>(ref->getData()))
			return r;
		if (auto r = std::dynamic_pointer_cast<const Modules::DataBase>(ref->getData()))
			return safe_cast<T>(r);
	}

	throw_dynamic_cast_error(typeid(T).name());
}

