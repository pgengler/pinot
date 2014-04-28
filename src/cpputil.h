#pragma once

#include <string>

#include <dirent.h>

int chmod(const std::string& path, mode_t mode);
int chown(const std::string& path, uid_t owner, gid_t group);
std::string getcwd();
DIR *opendir(const std::string& name);
int lstat(const std::string& path, struct stat *buf);
int stat(const std::string& path, struct stat *buf);
