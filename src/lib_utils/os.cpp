#include "os.hpp"
#include <stdexcept>

using namespace std;

#if _WIN32

#include <windows.h>
#include <direct.h> //chdir
bool setHighThreadPriority() {
	return SetThreadPriority(NULL, THREAD_PRIORITY_TIME_CRITICAL);
}

bool dirExists(std::string path) {
	DWORD attributes = GetFileAttributesA(path.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES)
		return false;

	return attributes & FILE_ATTRIBUTE_DIRECTORY ? true : false;
}

void mkdir(std::string path) {
	if(!CreateDirectoryA(path.c_str(), nullptr))
		throw std::runtime_error("couldn't create dir \"" + path + "\": please check you have sufficient permissions");
}

void moveFile(std::string src, std::string dst) {
	if(!MoveFileA(src.c_str(), dst.c_str())) {
		if (GetLastError() == ERROR_ALREADY_EXISTS) {
			DeleteFileA(dst.c_str());
		}
		throw std::runtime_error("can't move file");
	}
}

void changeDir(std::string path) {
	if (chdir(path.c_str()) < 0)
		throw std::runtime_error("can't change to dir '" + path + "'");
}

string thisExeDir() {
	char buffer[4096] {};

	auto r = GetModuleFileNameA(nullptr, buffer, sizeof buffer);
	if(r == 0)
		throw runtime_error("GetModuleFileNameA failure");

	buffer[r] = 0;

	string path = buffer;

	while(path.back() != '\\')
		path.pop_back();

	return path;
}

struct DynLibWin : DynLib {
	DynLibWin(const char* name) : handle(LoadLibrary(name)) {
		if(!handle) {
			string msg = "can't load '";
			msg += name;
			msg += "'";
			throw runtime_error(msg);
		}
	}

	~DynLibWin() {
		FreeLibrary(handle);
	}

	virtual void* getSymbol(const char* name) {
		auto func = GetProcAddress(handle, name);
		if(!func) {
			string msg = "can't find symbol '";
			msg += name;
			msg += "'";
			throw runtime_error(msg);
		}

		return (void*)func;
	}

	HMODULE const handle;
};

unique_ptr<DynLib> loadLibrary(const char* name) {
	return make_unique<DynLibWin>(name);
}

#else

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

bool dirExists(std::string path) {
	struct stat sb;
	return stat(path.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode);
}

void mkdir(std::string path) {
	if(::mkdir(path.c_str(), 0755) != 0)
		throw std::runtime_error("couldn't create dir \"" + path + "\": please check you have sufficient permissions");
}

void moveFile(std::string src, std::string dst) {
	if(system(("mv " +  src + " " +  dst).c_str()) != 0)
		throw std::runtime_error("can't move file");
}

void changeDir(std::string path) {
	if (chdir(path.c_str()) < 0)
		throw std::runtime_error("can't change to dir '" + path + "'");
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


#endif
