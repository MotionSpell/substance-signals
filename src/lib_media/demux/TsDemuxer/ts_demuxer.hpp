#pragma once

struct TsDemuxerConfig {
	enum { AUTO = 0, NONE = -1 };

	struct Pid {
		int pid = AUTO;
		int type = NONE;
	};

	static constexpr Pid AUTO_VIDEO = { AUTO, 1 };
	static constexpr Pid AUTO_AUDIO = { AUTO, 2 };

	Pid pids[8] = { AUTO_VIDEO, AUTO_AUDIO };
};

