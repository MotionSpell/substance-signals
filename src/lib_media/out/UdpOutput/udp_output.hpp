#pragma once

struct UdpOutputConfig {
	int ipAddr[4];
	int port;
	int bitrate = 0; // in bps. 0 means "send everything immediately"
};

