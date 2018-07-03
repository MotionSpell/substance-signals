#pragma once

#include <cstdint>
#include <cstddef> // size_t

namespace Modules {

struct IData {
	virtual ~IData() {}
	virtual bool isRecyclable() const = 0;
	virtual uint8_t* data() = 0;
	virtual const uint8_t* data() const = 0;
	virtual uint64_t size() const = 0;
	virtual void resize(size_t size) = 0;
};

}

