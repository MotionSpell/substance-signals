#pragma once

#include <csignal>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <thread>

#define TESTS
// generate a file-unique identifier, based on current line
#define unittestSuffix(suffix, prettyName) \
	static void testFunction##suffix(); \
	int g_isRegistered##suffix = Tests::RegisterTest(&testFunction##suffix, prettyName, g_isRegistered##suffix); \
	static void testFunction##suffix()

#define unittestLine(counter, prettyName) \
	unittestSuffix(counter, prettyName)

#define unittest(prettyName) \
	unittestLine(__COUNTER__, prettyName)

namespace Tests {
inline void Fail(char const* file, int line, const char* msg) {
	std::cerr << "TEST FAILED: " << file << "(" << line << "): " << msg << std::endl;
	std::raise(SIGABRT);
}

#define ASSERT(expr) \
	if (!(expr)) { \
		std::stringstream exprStringStream; \
		exprStringStream << "assertion failed: " << #expr; \
		Fail(__FILE__, __LINE__, exprStringStream.str().c_str()); \
	}

#define ASSERT_EQUALS(expected, actual) \
	if ((expected) != (actual)) { \
		std::stringstream ss; \
		ss << "assertion failed for expression: '" << #actual << "' , expected '" << (expected) << "' got '" << (actual) << "'"; \
		Fail(__FILE__, __LINE__, ss.str().c_str()); \
	}

int RegisterTest(void (*f)(), const char* testName, int& dummy);
void RunAll();

inline void Test(const std::string &name) {
	std::cout << std::endl << "[ ***** " << name.c_str() << " ***** ]" << std::endl;
}

}
