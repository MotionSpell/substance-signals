#include "tests/tests.hpp"

#include <map>
std::string expandVars(std::string input, std::map<std::string,std::string> const& values);

using namespace std;

unittest("expandVars: passthrough") {
	map<string, string> empty;
	ASSERT_EQUALS("hello", expandVars("hello", empty));
}

unittest("expandVars: simple") {
	map<string, string> vars;
	vars["who"] = "world";
	ASSERT_EQUALS("hello world", expandVars("hello $who$", vars));
}

unittest("expandVars: many") {
	map<string, string> vars;
	vars["who1"] = "John";
	vars["who2"] = "Doe";
	vars["when"] = "today";
	vars["what"] = "happy";
	ASSERT_EQUALS("JohnDoe is happy today", expandVars("$who1$$who2$ is $what$ $when$", vars));
}

unittest("expandVars: unexisting") {
	map<string, string> vars;
	vars["who"] = "world";
	ASSERT_THROWN(expandVars("hello $what$", vars));
}

unittest("expandVars: incomplete") {
	map<string, string> vars;
	vars["my_very_long_variable_name"] = "world";
	ASSERT_THROWN(expandVars("hello $my_very_long_vari", vars));
}
