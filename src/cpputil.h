#pragma once

#include <string>
#include <vector>
#include <cstring>

#include <sys/types.h>
#include <dirent.h>

int access(const std::string& pathname, int amode);
std::string basename(const std::string& path);
int chdir(const std::string& path);
int chmod(const std::string& path, mode_t mode);
int chown(const std::string& path, uid_t owner, gid_t group);
std::string dirname(const std::string& path);
int execvp(const std::string& file, const std::vector<std::string>& argv, char ***buf);
FILE* fopen(const std::string& path, const std::string& mode);
std::string getcwd();
int lstat(const std::string& path, struct stat *buf);
int mkstemp(std::string& name_template);
DIR *opendir(const std::string& name);
std::string realpath(const std::string& path);
int stat(const std::string& path, struct stat *buf);
int unlink(const std::string& path);
