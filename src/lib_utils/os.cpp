#include "os.hpp"
#include <stdexcept>

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

#else

#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h> //chdir

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

#endif
