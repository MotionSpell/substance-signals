#pragma once

struct TsDemuxerConfig {
	enum { ANY = 0 };
	enum { NONE = -1, VIDEO = 1, AUDIO = 2 };

	struct Pid {
		int pid = ANY;
		int type = NONE;
	};

	static constexpr Pid ANY_VIDEO() {
		return { ANY, VIDEO };
	};
	static constexpr Pid ANY_AUDIO() {
		return { ANY, AUDIO };
	};

	Pid pids[8] = { ANY_VIDEO(), ANY_AUDIO() };
};

