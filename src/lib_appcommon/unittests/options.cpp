#include "tests/tests.hpp"
#include "lib_appcommon/options.hpp"

#define NELEMENTS(a) \
  sizeof(a)/sizeof(*(a))

unittest("CmdLineOptions: no flags") {
	int a, b;
	CmdLineOptions opt;
	opt.add("a", "aaa", &a);
	opt.add("b", "bbb", &b);

	const char* argv[] = {
		"progname", "myfile.dat"
	};
	auto res = opt.parse(NELEMENTS(argv), argv);
	ASSERT_EQUALS(std::vector<std::string>({"myfile.dat"}), res);
}

unittest("CmdLineOptions: short names") {
	std::string s;
	int n = -1;
	CmdLineOptions opt;
	opt.add("i", "inputPath", &s);
	opt.add("n", "numCores", &n);

	const char* argv[] = {
		"progname", "-i", "hello", "-n", "34"
	};
	opt.parse(NELEMENTS(argv), argv);
	ASSERT_EQUALS("hello", s);
	ASSERT_EQUALS(34, n);
}

unittest("CmdLineOptions: long names") {
	std::string s;
	int n = -1;
	CmdLineOptions opt;
	opt.add("i", "inputPath", &s);
	opt.add("n", "numCores", &n);

	const char* argv[] = {
		"progname", "--inputPath", "/dev/null", "--numCores", "8"
	};
	opt.parse(NELEMENTS(argv), argv);
	ASSERT_EQUALS("/dev/null", s);
	ASSERT_EQUALS(8, n);
}

unittest("CmdLineOptions: vector and flag") {
	std::vector<std::string> values;
	bool keepGoing = false;
	CmdLineOptions opt;
	opt.add("f", "flags", &values);
	opt.addFlag("k", "keepGoing", &keepGoing);

	const char* argv[] = {
		"cmd", "--flags", "red", "green", "blue", "-k"
	};
	opt.parse(NELEMENTS(argv), argv);
	ASSERT_EQUALS(std::vector<std::string>({"red", "green", "blue"}), values);
	ASSERT_EQUALS(true, keepGoing);
}

unittest("CmdLineOptions: unknown option") {
	bool keepGoing = false;
	CmdLineOptions opt;
	opt.add("k", "keepGoing", &keepGoing);

	const char* argv[] = {
		"cmd", "--evil"
	};
	ASSERT_THROWN(opt.parse(NELEMENTS(argv), argv));
}

