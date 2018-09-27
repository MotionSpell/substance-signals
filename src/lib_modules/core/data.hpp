#pragma once

#include <cstdint>
#include <cstddef> // size_t

template<typename T>
struct span {
	T* ptr;
	size_t len;

	span() = default;

	template<size_t N>
	span(T (&tab)[N]) : ptr(tab), len(N) {
	}

	span(T* ptr_, size_t len_) : ptr(ptr_), len(len_) {
	}

	void operator+=(size_t n) {
		ptr += n;
		len -= n;
	}

	T& operator [] (int i) {
		return ptr[i];
	}

	T* begin() {
		return ptr;
	}

	T* end() {
		return ptr + len;
	}
};

typedef span<uint8_t> Span;
typedef span<const uint8_t> SpanC;

namespace Modules {

struct IData {
	virtual ~IData() {}
	virtual bool isRecyclable() const = 0;
	virtual Span data() = 0;
	virtual SpanC data() const = 0;
	virtual void resize(size_t size) = 0;
};

}

