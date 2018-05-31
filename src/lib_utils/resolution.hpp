#pragma once

#include "format.hpp"

struct Resolution {
	Resolution(int w = 0, int h = 0) : width(w), height(h) {
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
	int width, height;
};
