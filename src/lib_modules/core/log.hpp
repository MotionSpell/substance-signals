#pragma once

#include "lib_utils/log.hpp"
#include "lib_utils/format.hpp"

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
						g_Log->log(l, format("%sLast message repeated %s times.", id, lastMsgCount).c_str());
					}

					g_Log->log(l, format("%s%s", id, msg).c_str());
					lastLevel = l;
					lastMsg = msg;
					lastMsgCount = 0;
				}
			}
		}

	private:
		Level level = Info;
		Level lastLevel = Debug;
		std::string lastMsg;
		uint64_t lastMsgCount = 0;
};

}
