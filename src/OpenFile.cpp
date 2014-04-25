#include "OpenFile.h"

#include "proto.h"

OpenFile::OpenFile()
: fileage(nullptr),
  filebot(nullptr),
  edittop(nullptr),
  current(nullptr),
  current_stat(nullptr),
  last_action(OTHER)
{
	// nothing to do here
}

OpenFile::~OpenFile()
{
	if (fileage != nullptr) {
		free_filestruct(fileage);
	}

	if (current_stat != nullptr) {
		delete current_stat;
	}
}
