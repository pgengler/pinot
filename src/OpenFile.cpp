#include "OpenFile.h"

#include "proto.h"

OpenFile::OpenFile()
: filename(nullptr),
  fileage(nullptr),
  filebot(nullptr),
  edittop(nullptr),
  current(nullptr),
  current_stat(nullptr),
  last_action(OTHER),
  lock_filename(nullptr)
{
	// nothing to do here
}

OpenFile::~OpenFile()
{
	if (filename != nullptr) {
		free(filename);
	}

	if (fileage != nullptr) {
		free_filestruct(fileage);
	}

DEBUG_LOG("this == " << static_cast<void*>(this) << "; current_stat == " << static_cast<void*>(current_stat));

	if (current_stat != nullptr) {
		delete current_stat;
	}
}
