#include "cpputil.h"
#include "proto.h"

#include <unistd.h>

std::string getcwd()
{
	char *buf = (char *)malloc((PATH_MAX + 1) * sizeof(char));
	buf = getcwd(buf, PATH_MAX + 1);

	std::string cwd(buf);
	free(buf);

	return cwd;
}

int lstat(const std::string& path, struct stat *buf)
{
	return lstat(path.c_str(), buf);
}

int stat(const std::string& path, struct stat *buf)
{
	return stat(path.c_str(), buf);
}
