#pragma once

struct AttributePresentationTime {
	enum { TypeId = 0x35A12022 };
	int64_t presentationTime;
};

struct AttributeDecodingTime {
	enum { TypeId = 0x5DF434D0 };
	int64_t decodingTime;
};

struct AttributeCueFlags {
	enum { TypeId = 0x172C1D4F };
	bool discontinuity;
	bool keyframe;
};

