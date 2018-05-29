#pragma once

#include <string>

bool setHighThreadPriority();

// filesystem
bool dirExists(std::string path);
void mkdir(std::string path);
void moveFile(std::string src, std::string dst);
void changeDir(std::string path);

