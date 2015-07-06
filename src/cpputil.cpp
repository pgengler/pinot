#include "cpputil.h"
#include "proto.h"

#include <unistd.h>

int access(const std::string& pathname, int mode)
{
	return ::access(pathname.c_str(), mode);
}

std::string basename(const std::string& path)
{
	return std::string(::basename(path.c_str()));
}

int chdir(const std::string& path)
{
	return ::chdir(path.c_str());
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

int execvp(const std::string& file, const std::vector<std::string>& argv, char ***buf)
{
	*buf = (char **)malloc((argv.size() + 1) * sizeof(char *));
	char **args = *buf;
	size_t pos = 0;
	for (auto arg : argv) {
		args[pos++] = mallocstrcpy(NULL, arg.c_str());
	}
	args[pos] = NULL;

	return execvp(file.c_str(), args);
}

FILE* fopen(const std::string& path, const std::string& mode)
{
	return ::fopen(path.c_str(), mode.c_str());
}

size_t fwrite(const std::string& string, FILE *stream)
{
	return fwrite(string.c_str(), sizeof(char), string.length(), stream);
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
	char *buffer = (char *)malloc((name_template.length() + 1) * sizeof(char));
	strncpy(buffer, name_template.c_str(), name_template.length());
	buffer[ name_template.length() ] = '\0';
	int fd = mkstemp(buffer);
	name_template = buffer;
	free(buffer);
	return fd;
}

DIR *opendir(const std::string& name)
{
	return opendir(name.c_str());
}

std::string realpath(const std::string& path)
{
	char *real_path = realpath(path.c_str(), NULL);
	if (!real_path) {
		DEBUG_LOG("Failed to get real path for '" << path << "'; error is: " << strerror(errno));
		return "";
	}
	std::string real_path_str = std::string(real_path);
	free(real_path);

	return real_path_str;
}

int stat(const std::string& path, struct stat *buf)
{
	return stat(path.c_str(), buf);
}

int unlink(const std::string& pathname)
{
	return ::unlink(pathname.c_str());
}
