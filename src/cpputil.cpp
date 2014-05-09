#include "cpputil.h"
#include "proto.h"

#include <unistd.h>

int access(const std::string& path, int amode)
{
	return access(path.c_str(), amode);
}

std::string basename(const std::string& path)
{
	return std::string(::basename(path.c_str()));
}

int chmod(const std::string& path, mode_t mode)
{
	return chmod(path.c_str(), mode);
}

int chown(const std::string& path, uid_t owner, gid_t group)
{
	return chown(path.c_str(), owner, group);
}

std::string dirname(const std::string& path)
{
	return std::string(::dirname(path.c_str()));
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

int mkstemp(std::string& name_template)
{
	char *buffer = (char *)malloc(name_template.length() * sizeof(char));
	int fd = mkstemp(buffer);
	name_template = buffer;
	return fd;
}

DIR *opendir(const std::string& name)
{
	return opendir(name.c_str());
}

int stat(const std::string& path, struct stat *buf)
{
	return stat(path.c_str(), buf);
}

int unlink(const std::string& path)
{
	return unlink(path.c_str());
}
