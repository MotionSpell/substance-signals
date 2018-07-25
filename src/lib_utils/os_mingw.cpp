#include "os.hpp"
#include <stdexcept>

using namespace std;

#include <windows.h>
#include <direct.h> //chdir
bool setHighThreadPriority() {
	return SetThreadPriority(NULL, THREAD_PRIORITY_TIME_CRITICAL);
}

bool dirExists(string path) {
	DWORD attributes = GetFileAttributesA(path.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES)
		return false;

	return attributes & FILE_ATTRIBUTE_DIRECTORY ? true : false;
}

void mkdir(string path) {
	if(!CreateDirectoryA(path.c_str(), nullptr))
		throw runtime_error("couldn't create dir \"" + path + "\": please check you have sufficient permissions");
}

void moveFile(string src, string dst) {
	if(!MoveFileA(src.c_str(), dst.c_str())) {
		if (GetLastError() == ERROR_ALREADY_EXISTS) {
			DeleteFileA(dst.c_str());
		}
		throw runtime_error("can't move file");
	}
}

void changeDir(string path) {
	if (chdir(path.c_str()) < 0)
		throw runtime_error("can't change to dir '" + path + "'");
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

