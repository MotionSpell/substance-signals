#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <csignal>
#include <climits>
#include "tests.hpp"

namespace {
typedef void (*TestFunction)();

struct UnitTest {
	void (*fn)();
	const char* name;

	// To be "FIRST" class, a test must be:
	//
	// * Fast: the total time of the unit tests must be less than 30s,
	//         otherwise developers will run them less often.
	//
	// * Isolated: the outcome of the test must only depend on the behavior of a
	//             single module.
	//
	// * Repeatable: tests must be deterministic, and must not depend on the
	//               environment (e.g must not interact with the outside world).
	//
	// * Self-Validating: the test result must be "Yes" or "No". It must not be
	//                    "Maybe", or some number that must be manually checked.
	//
	// * Timely: tests must look like they were written *before* the production
	//           code: this way, the API of the production code is designed
	//           from the point of view of the user of the API, not the implementer.
	//
	int type; // 0:first class, 1:second class, 2:fuzztest
};

UnitTest g_AllTests[65536];
int g_NumTests;

struct Filter {
	int minIdx = 0;
	int maxIdx = INT_MAX;
	bool noSecondClass = true;
	int fuzzIdx = -1;
	std::string fuzzPath;
};

void listAll() {
	for (int i = 0; i < g_NumTests; ++i) {
		std::cout << "Test #" << i << ": " << g_AllTests[i].name << std::endl;
	}
}

bool startsWith(std::string s, std::string prefix) {
	return s.substr(0, prefix.size()) == prefix;
}

bool matches(Filter filter, int idx) {
	if(idx < filter.minIdx)
		return false;
	if(idx > filter.maxIdx)
		return false;
	if(g_AllTests[idx].type == 2)
		return false;
	if(filter.noSecondClass && g_AllTests[idx].type == 1)
		return false;
	if(startsWith(g_AllTests[idx].name, "[DISABLED]"))
		return false;
	return true;
}

static uint8_t fuzzBuffer[1024];

void RunAll(Filter filter) {
	if(filter.fuzzIdx >= 0) {
		std::ifstream fp(filter.fuzzPath);
		fp.read((char*)fuzzBuffer, sizeof fuzzBuffer);
		g_AllTests[filter.fuzzIdx].fn();
	} else {
		for(int i=0; i < g_NumTests; ++i) {
			if(matches(filter, i)) {
				std::cout << "#" << i << ": " << g_AllTests[i].name << std::endl;
				g_AllTests[i].fn();
			}
		}
	}
}
}

namespace Tests {

void Fail(char const* file, int line, const char* msg) {
	std::cerr << "TEST FAILED: " << file << "(" << line << "): " << msg << std::endl;
	std::raise(SIGABRT);
}

int RegisterTest(void (*fn)(), const char* testName, int type, int&) {
	g_AllTests[g_NumTests].fn = fn;
	g_AllTests[g_NumTests].name = testName;
	g_AllTests[g_NumTests].type = type;
	++g_NumTests;
	return 0;
}

void GetFuzzTestData(uint8_t const*& ptr, size_t& len) {
	ptr = fuzzBuffer;
	len = sizeof fuzzBuffer;
}
}

int main(int argc, const char* argv[]) {
	int i = 1;
	auto popWord = [&]() -> std::string {
		if(i >= argc)
			throw std::runtime_error("unexpected end of command line");
		return argv[i++];
	};

	Filter filter;

	while(i < argc) {
		auto const word = popWord();

		if(word == "--list" || word == "-l") {
			listAll();
			return 0;
		} else if(word == "--only") {
			auto idx = atoi(popWord().c_str());
			filter.minIdx = idx;
			filter.maxIdx = idx;
			filter.noSecondClass = false;
		} else if(word == "--range") {
			filter.minIdx =  atoi(popWord().c_str());
			filter.maxIdx =  atoi(popWord().c_str());
		} else if(word == "--second-class") {
			filter.noSecondClass = false;
		} else if(word == "--fuzz") {
			filter.fuzzIdx = atoi(popWord().c_str());
			filter.fuzzPath = popWord().c_str();
		}
	}

	RunAll(filter);
	return 0;
}
