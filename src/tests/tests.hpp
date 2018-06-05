#pragma once

#include <sstream>
#include <vector>

// generate a file-unique identifier, based on current line
#define unittestSuffix(suffix, prettyName, secondClass) \
	static void testFunction##suffix(); \
	static int g_isRegistered##suffix = Tests::RegisterTest(&testFunction##suffix, prettyName, secondClass, g_isRegistered##suffix); \
	static void testFunction##suffix()

#define unittestLine(counter, prettyName, secondClass) \
	unittestSuffix(counter, prettyName, secondClass)

#define unittest(prettyName) \
	unittestLine(__COUNTER__, prettyName, 0)

#define secondclasstest(prettyName) \
	unittestLine(__COUNTER__, prettyName, 1)

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

void Fail(char const* file, int line, const char* msg);

template<typename T>
std::string ToString(T const& val) {
	std::stringstream ss;
	ss << val;
	return ss.str();
}

template<typename T>
inline void Assert(char const* file, int line, const char* caption, T const& result) {
	if (!result) {
		std::string msg;
		msg += "assertion failed: ";
		msg += caption;
		::Tests::Fail(file, line, msg.c_str());
	}
}

template<typename T, typename U>
inline void AssertEquals(char const* file, int line, const char* caption, T const& expected, U const& actual) {
	if (expected != actual) {
		auto sExpected = ToString(expected);
		auto sActual = ToString(actual);
		bool multiline = sExpected.size() > 40 || sActual.size() > 40;

		std::string msg;
		msg += "assertion failed for expression: '" + std::string(caption) + "'. ";
		if(multiline)
			msg += "\n";
		msg += "expected '" + sExpected + "' ";
		if(multiline)
			msg += "\n     ";
		msg += "got '" + sActual + "'";
		::Tests::Fail(file, line, msg.c_str());
	}
}

#define ASSERT(expr) \
  ::Tests::Assert(__FILE__, __LINE__, #expr, expr)

#define ASSERT_EQUALS(expected, actual) \
  ::Tests::AssertEquals(__FILE__, __LINE__, #actual, expected, actual)

#define ASSERT_THROWN(expr) \
	try { \
		expr; \
		::Tests::Fail(__FILE__, __LINE__, "Missing exception: " #expr); \
		ASSERT(0); \
	} catch (...) { \
	} \

int RegisterTest(void (*f)(), const char* testName, bool secondClass, int& dummy);

}
