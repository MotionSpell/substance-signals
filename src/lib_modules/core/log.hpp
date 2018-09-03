#pragma once

#include "lib_utils/log.hpp"
#include <typeinfo>

#define LOG_MSG_REPETITION_MAX 100

namespace Modules {

class LogRepetition {
	public:

		template<typename... Arguments>
		void msg(Level l, const std::string& id, const std::string& fmt, Arguments... args) {
			if ((level != Quiet) && (l <= level)) {
				auto const msg = format(fmt, args...);
				if (lastMsgCount < LOG_MSG_REPETITION_MAX && l == lastLevel && msg == lastMsg) {
					lastMsgCount++;
				} else {
					if (lastMsgCount) {
						Log::msg(l, "%sLast message repeated %s times.", id, lastMsgCount);
					}

					Log::msg(l, "%s%s", id, msg);
					lastLevel = l;
					lastMsg = msg;
					lastMsgCount = 0;
				}
			}
		}

	private:
		Level level = Log::getLevel(), lastLevel = Debug;
		std::string lastMsg;
		uint64_t lastMsgCount = 0;
};

}
