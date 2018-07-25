#include "os.hpp"
#include <stdexcept>

using namespace std;

#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h> //chdir
#include <dlfcn.h> // dlopen
#include <libgen.h> // dirname

bool setHighThreadPriority() {
	sched_param sp {};
	sp.sched_priority = 1;
	if (pthread_setschedparam(pthread_self(), SCHED_RR, &sp))
		return false;

	return true;
}

bool dirExists(string path) {
	struct stat sb;
	return stat(path.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode);
}

void mkdir(string path) {
	if(::mkdir(path.c_str(), 0755) != 0)
		throw runtime_error("couldn't create dir \"" + path + "\": please check you have sufficient permissions");
}

void moveFile(string src, string dst) {
	if(system(("mv " +  src + " " +  dst).c_str()) != 0)
		throw runtime_error("can't move file");
}

void changeDir(string path) {
	if (chdir(path.c_str()) < 0)
		throw runtime_error("can't change to dir '" + path + "'");
}

string thisExeDir() {
	char buffer[4096] {};
	auto r = readlink("/proc/self/exe", buffer, sizeof buffer);
	if(r == -1)
		throw runtime_error("can't read link /proc/self/exe");

	buffer[r] = 0;

	return dirname(buffer) + string("/");
}

struct DynLibGnu : DynLib {
	DynLibGnu(const char* name) : handle(dlopen(name, RTLD_NOW)) {
		if(!handle) {
			string msg = "can't load '";
			msg += name;
			msg += "' (";
			msg += dlerror();
			msg += ")";
			throw runtime_error(msg);
		}
	}

	~DynLibGnu() {
		dlclose(handle);
	}

	virtual void* getSymbol(const char* name) {
		auto func = dlsym(handle, name);
		if(!func) {
			string msg = "can't find symbol '";
			msg += name;
			msg += "'";
			throw runtime_error(msg);
		}

		return func;
	}

	void* const handle;
};

unique_ptr<DynLib> loadLibrary(const char* name) {
	return make_unique<DynLibGnu>(name);
}

