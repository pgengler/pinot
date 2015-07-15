#pragma once

#include <vector>

#include <sys/types.h>
#include <dirent.h>

#include "PinotString.h"
using pinot::string;

int access(string pathname, int amode);
string basename(string path);
int chdir(string path);
int chmod(string path, mode_t mode);
int chown(string path, uid_t owner, gid_t group);
string dirname(string path);
int execvp(string file, const std::vector<string>& argv);
FILE* fopen(string path, string mode);
size_t fwrite(string string, FILE *stream);
string getcwd();
int lstat(string path, struct stat *buf);
int mkstemp(string& name_template);
DIR *opendir(string name);
string realpath(string path);
int stat(string path, struct stat *buf);
int unlink(string path);
