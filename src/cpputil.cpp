#include "cpputil.h"
#include "proto.h"

#include <unistd.h>

int chmod(const std::string& path, mode_t mode)
{
	return chmod(path.c_str(), mode);
}

int chown(const std::string& path, uid_t owner, gid_t group)
{
	return chown(path.c_str(), owner, group);
}

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

DIR *opendir(const std::string& name)
{
	return opendir(name.c_str());
}

int stat(const std::string& path, struct stat *buf)
{
	return stat(path.c_str(), buf);
}
