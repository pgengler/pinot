#pragma once

#include <string>

int chmod(const std::string& path, mode_t mode);
int chown(const std::string& path, uid_t owner, gid_t group);
std::string getcwd();
int lstat(const std::string& path, struct stat *buf);
int stat(const std::string& path, struct stat *buf);
