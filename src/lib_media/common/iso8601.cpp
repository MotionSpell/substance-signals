#include <string>
#include <sstream>
#include <cctype>
#include <stdexcept>
#include <stdint.h>

using namespace std;

int64_t parseIso8601Period(string input) {
	auto s = input.c_str();

	if (*s++ != 'P')
		throw runtime_error("invalid period, must start with 'P'");

	if (*s++ != 'T')
		throw runtime_error("not supported: dates");

	auto parseDecimal = [&]() {
		int64_t denom = 1;
		int64_t r = 0;
		while (isdigit(*s)) {
			r *= 10;
			r += *s++ - '0';
		}
		if(*s == '.') {
			++s;
			while (isdigit(*s)) {
				r *= 10;
				r += *s++ - '0';
				denom *= 10;
			}
		}
		return double(r) / denom;
	};

	int64_t seconds = 0;

	while (*s) {
		auto value = parseDecimal();

		switch (*s++) {
		case 'H':
			seconds += int(value * 3600);
			break;
		case 'M':
			seconds += int(value * 60);
			break;
		case 'S':
			seconds += int(value);
			break;
		default:
			throw runtime_error("invalid period, unrecognized char");
		}
	}

	return seconds;
}

