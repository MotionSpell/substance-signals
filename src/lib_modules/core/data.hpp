#pragma once

#include <cstdint>
#include <cstddef> // size_t

struct Span {
	uint8_t* ptr;
	size_t len;
};

struct SpanC {
	const uint8_t* ptr;
	size_t len;
};

namespace Modules {

struct IData {
	virtual ~IData() {}
	virtual bool isRecyclable() const = 0;
	virtual Span data() = 0;
	virtual SpanC data() const = 0;
	virtual void resize(size_t size) = 0;
};

}

