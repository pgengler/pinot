#pragma once

#include <string>
using std::string;

#include "types.h"

class OpenFile
{
	public:
		OpenFile();
		virtual ~OpenFile();

		/* The current file's name. */
		string filename;

		/* The current file's first line. */
		filestruct *fileage;

		/* The current file's last line. */
		filestruct *filebot;

		/* The current top of the edit window. */
		filestruct *edittop;

		/* The current file's current line. */
		filestruct *current;

		/* The current file's total number of characters. */
		size_t totsize;

		/* The current file's x-coordinate position. */
		size_t current_x;

		/* The current file's place we want. */
		size_t placewewant;

		/* The current file's y-coordinate position. */
		ssize_t current_y;

		/* Whether the current file has been modified. */
		bool modified;

		/* Whether the mark is on in the current file. */
		bool mark_set;

		/* The current file's beginning marked line, if any. */
		filestruct *mark_begin;

		/* The current file's beginning marked line's x-coordinate position, if any. */
		size_t mark_begin_x;

		/* The current file's format. */
		FileFormat fmt;

		/* The current file's stat. */
		struct stat *current_stat;

		/* Top of the undo list */
		undo *undotop;

		/* The current (i.e. n ext) level of undo */
		undo *current_undo;

		UndoType last_action;

		/* The path of the lockfile, if we created one */
		string lock_filename;

		/* The syntax class for this file, if any */
		Syntax *syntax;

		/* The current file's associated colors. */
		ColorList colorstrings;

};
