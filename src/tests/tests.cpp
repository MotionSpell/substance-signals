#include <iostream>
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
	// * Fast
	// * Isolated
	// * Repeatable
	// * Self-Validating
	// * Timely
	bool secondClass;
};

UnitTest g_AllTests[65536];
int g_NumTests;

struct Filter {
	int minIdx = 0;
	int maxIdx = INT_MAX;
	bool noSecondClass = true;
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
	if(filter.noSecondClass && g_AllTests[idx].secondClass)
		return false;
	if(startsWith(g_AllTests[idx].name, "[DISABLED]"))
		return false;
	return true;
}

void RunAll(Filter filter) {
	for(int i=0; i < g_NumTests; ++i) {
		if(matches(filter, i)) {
			std::cout << "#" << i << ": " << g_AllTests[i].name << std::endl;
			g_AllTests[i].fn();
		}
	}
	std::cout << "-- Tests were run successfully" << std::endl;
}
}

namespace Tests {

void Fail(char const* file, int line, const char* msg) {
	std::cerr << "TEST FAILED: " << file << "(" << line << "): " << msg << std::endl;
	std::raise(SIGABRT);
}

int RegisterTest(void (*fn)(), const char* testName, bool secondClass, int&) {
	g_AllTests[g_NumTests].fn = fn;
	g_AllTests[g_NumTests].name = testName;
	g_AllTests[g_NumTests].secondClass = secondClass;
	++g_NumTests;
	return 0;
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
		}
	}

	RunAll(filter);
	return 0;
}
