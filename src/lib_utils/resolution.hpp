#pragma once

#include "format.hpp"

struct Resolution {
	Resolution(unsigned int w = 0, unsigned int h = 0) : width(w), height(h) {
	}
	bool operator==(Resolution const& other) const {
		return width == other.width && height == other.height;
	}
	bool operator!=(Resolution const& other) const {
		return !(*this == other);
	}
	Resolution operator/(const int div) const {
		return Resolution(this->width / div, this->height / div);
	}
	std::string toString() const {
		return format("%sx%s", width, height);
	}
	unsigned int width, height;
};
