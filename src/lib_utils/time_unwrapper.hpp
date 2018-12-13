#pragma once

#include <cstdint>

struct TimeUnwrapper {
	int64_t unwrap(int64_t time) {
		if(!m_init) {
			m_init = true;
			m_when = time;
		}

		while(time < m_when - WRAP_PERIOD/2)
			time += WRAP_PERIOD;

		if(time > m_when)
			m_when = time;

		return time;
	}

	bool m_init = false;
	int64_t m_when = 0;

	int64_t WRAP_PERIOD = 1LL<<33;
};

