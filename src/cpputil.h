#pragma once

#include <string>

std::string getcwd();
int lstat(const std::string& path, struct stat *buf);
int stat(const std::string& path, struct stat *buf);
