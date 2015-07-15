#include "cpputil.h"
#include "proto.h"

#include <cstring>

#include <unistd.h>

int access(string pathname, int mode)
{
	return ::access(pathname.c_str(), mode);
}

string basename(string path)
{
	return string(::basename(path.c_str()));
}

int chdir(string path)
{
	return ::chdir(path.c_str());
}

int chmod(string path, mode_t mode)
{
	return chmod(path.c_str(), mode);
}

int chown(string path, uid_t owner, gid_t group)
{
	return chown(path.c_str(), owner, group);
}

string dirname(string path)
{
	return string(::dirname(path.c_str()));
}

int execvp(string file, const std::vector<string>& argv)
{
	std::vector<const char *> args;
	for (auto arg : argv) {
		args.push_back(arg.c_str());
	}
	args.push_back(NULL);

	return execvp(file.c_str(), (char* const *)&args[0]);
}

FILE* fopen(string path, string mode)
{
	return ::fopen(path.c_str(), mode.c_str());
}

size_t fwrite(string string, FILE *stream)
{
	return fwrite(string.c_str(), sizeof(char), string.length(), stream);
}

string getcwd()
{
	char *buf = new char[PATH_MAX + 1];
	buf = getcwd(buf, PATH_MAX + 1);

	string cwd(buf);
	delete[] buf;

	return cwd;
}

int lstat(string path, struct stat *buf)
{
	return lstat(path.c_str(), buf);
}

int mkstemp(string& name_template)
{
	char *buffer = new char[name_template.length() + 1];
	::strncpy(buffer, name_template.c_str(), name_template.length());
	buffer[ name_template.length() ] = '\0';
	int fd = mkstemp(buffer);
	name_template = buffer;
	delete[] buffer;
	return fd;
}

DIR *opendir(string name)
{
	return opendir(name.c_str());
}

string realpath(string path)
{
	char *real_path = realpath(path.c_str(), NULL);
	if (!real_path) {
		DEBUG_LOG("Failed to get real path for '" << path << "'; error is: " << strerror(errno));
		return "";
	}
	string real_path_str = string(real_path);
	free(real_path);

	return real_path_str;
}

int stat(string path, struct stat *buf)
{
	return stat(path.c_str(), buf);
}

int unlink(string pathname)
{
	return ::unlink(pathname.c_str());
}
