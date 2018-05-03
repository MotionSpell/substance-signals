#pragma once

#include <sstream>
#include <vector>

#define TESTS
// generate a file-unique identifier, based on current line
#define unittestSuffix(suffix, prettyName) \
	static void testFunction##suffix(); \
	static int g_isRegistered##suffix = Tests::RegisterTest(&testFunction##suffix, prettyName, g_isRegistered##suffix); \
	static void testFunction##suffix()

#define unittestLine(counter, prettyName) \
	unittestSuffix(counter, prettyName)

#define unittest(prettyName) \
	unittestLine(__COUNTER__, prettyName)

// TODO: find a way not to explicitly depend on 'vector'.
template<typename T>
inline std::ostream& operator<<(std::ostream& o, std::vector<T> iterable) {
	o << "[";
	bool first = true;
	for(auto& val : iterable) {
		if(!first)
			o << ", ";
		o << val;
		first = false;
	}
	o << "]";
	return o;
}

namespace Tests {

void Test(char const* name);
void Fail(char const* file, int line, const char* msg);

template<typename T>
inline void Assert(char const* file, int line, const char* caption, T const& result) {
	if (!result) {
		std::stringstream ss;
		ss << "assertion failed: " << caption;
		::Tests::Fail(file, line, ss.str().c_str());
	}
}

template<typename T, typename U>
inline void AssertEquals(char const* file, int line, const char* caption, T const& expected, U const& actual) {
	if (expected != actual) {
		std::stringstream ss;
		ss << "assertion failed for expression: '" << caption << "' , expected '" << expected << "' got '" << actual << "'";
		::Tests::Fail(file, line, ss.str().c_str());
	}
}

#define ASSERT(expr) \
  ::Tests::Assert(__FILE__, __LINE__, #expr, expr)

#define ASSERT_EQUALS(expected, actual) \
  ::Tests::AssertEquals(__FILE__, __LINE__, #actual, expected, actual)

#define ASSERT_THROWN(expr) \
	try { \
		expr; \
		ASSERT(0); \
	} catch (...) { \
	} \

int RegisterTest(void (*f)(), const char* testName, int& dummy);
void RunAll();

}
