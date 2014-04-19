/**************************************************************************
 *   pinot.h                                                              *
 *                                                                        *
 *   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007,  *
 *   2008, 2009 Free Software Foundation, Inc.                            *
 *   This program is free software; you can redistribute it and/or modify *
 *   it under the terms of the GNU General Public License as published by *
 *   the Free Software Foundation; either version 3, or (at your option)  *
 *   any later version.                                                   *
 *                                                                        *
 *   This program is distributed in the hope that it will be useful, but  *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU    *
 *   General Public License for more details.                             *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program; if not, write to the Free Software          *
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA            *
 *   02110-1301, USA.                                                     *
 *                                                                        *
 **************************************************************************/

#pragma once

#include <iostream>
#include <string>
#include <vector>
#include "macros.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "Keyboard.h"

#ifdef NEED_XOPEN_SOURCE_EXTENDED
#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED 1
#endif /* _XOPEN_SOURCE_EXTENDED */
#endif /* NEED_XOPEN_SOURCE_EXTENDED */

#ifdef __TANDEM
/* Tandem NonStop Kernel support. */
#include <floss.h>
#define PINOT_ROOT_UID 65535
#else
#define PINOT_ROOT_UID 0
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

/* Set a default value for PATH_MAX if there isn't one. */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef USE_SLANG
/* Slang support. */
#include <slcurses.h>
/* Slang curses emulation brain damage, part 3: Slang doesn't define the
 * curses equivalents of the Insert or Delete keys. */
#define KEY_DC SL_KEY_DELETE
#define KEY_IC SL_KEY_IC
/* Ncurses support. */
#elif defined(HAVE_NCURSESW_NCURSES_H)
#include <ncursesw/ncurses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#else
/* Curses support. */
#include <curses.h>
#endif /* CURSES_H */

#if defined(ENABLE_NLS) && defined(HAVE_LIBINTL_H)
#include <libintl.h>
#endif

#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pcreposix.h>
#include <setjmp.h>
#include <assert.h>

/* If no vsnprintf(), use the version from glib 2.x. */
#ifndef HAVE_VSNPRINTF
#include <glib.h>
#define vsnprintf g_vsnprintf
#endif

/* If no isblank(), iswblank(), strcasecmp(), strncasecmp(),
 * strcasestr(), strnlen(), getdelim(), or getline(), use the versions
 * we have. */
#ifndef HAVE_ISBLANK
#define isblank nisblank
#endif
#ifndef HAVE_ISWBLANK
#define iswblank niswblank
#endif
#ifndef HAVE_STRCASECMP
#define strcasecmp nstrcasecmp
#endif
#ifndef HAVE_STRNCASECMP
#define strncasecmp nstrncasecmp
#endif
#ifndef HAVE_STRCASESTR
#define strcasestr nstrcasestr
#endif
#ifndef HAVE_STRNLEN
#define strnlen nstrnlen
#endif
#ifndef HAVE_GETDELIM
#define getdelim ngetdelim
#endif
#ifndef HAVE_GETLINE
#define getline ngetline
#endif

/* Enumeration types. */
typedef enum {
	NIX_FILE, DOS_FILE, MAC_FILE
} file_format;

typedef enum {
	OVERWRITE, APPEND, PREPEND
} append_type;

typedef enum {
	UP_DIR, DOWN_DIR
} scroll_dir;

typedef enum {
	CENTER, NONE
} update_type;

typedef enum {
	CONTROL, META, FKEY, RAWINPUT
}  function_type;

typedef enum {
	ADD, DEL, REPLACE, SPLIT, UNSPLIT, CUT, UNCUT, ENTER, INSERT, OTHER
} undo_type;

typedef enum {
	PROMPT_BLANK_STRING = -2, PROMPT_ABORTED, PROMPT_ENTER_PRESSED, PROMPT_OTHER_KEY
} PromptResult;

typedef enum {
	YESNO_PROMPT_ABORTED = -1, YESNO_PROMPT_NO, YESNO_PROMPT_YES, YESNO_PROMPT_ALL, YESNO_PROMPT_UNKNOWN = 255
} YesNoPromptResult;

#include "syntax.h"

/* Structure types. */
typedef struct filestruct {
	char *data;
	/* The text of this line. */
	ssize_t lineno;
	/* The number of this line. */
	struct filestruct *next;
	/* Next node. */
	struct filestruct *prev;
	/* Previous node. */
	short *multidata;		/* Array of which multi-line regexes apply to this line */
} filestruct;

typedef struct partition {
	filestruct *fileage;
	/* The top line of this portion of the file. */
	filestruct *top_prev;
	/* The line before the top line of this portion of the file. */
	char *top_data;
	/* The text before the beginning of the top line of this portion
	 * of the file. */
	filestruct *filebot;
	/* The bottom line of this portion of the file. */
	filestruct *bot_next;
	/* The line after the bottom line of this portion of the
	 * file. */
	char *bot_data;
	/* The text after the end of the bottom line of this portion of
	 * the file. */
} partition;

typedef struct undo {
	ssize_t lineno;
	undo_type type;
	/* What type of undo was this */
	int begin;
	/* Where did this  action begin or end */
	char *strdata;
	/* String type data we will use for ccopying the affected line back */
	char *strdata2;
	/* Sigh, need this too it looks like */
	int xflags;
	/* Some flag data we need */

	/* Cut specific stuff we need */
	filestruct *cutbuffer;
	/* Copy of the cutbuffer */
	filestruct *cutbottom;
	/* Copy of cutbottom */
	bool mark_set;
	/* was the marker set when we cut */
	bool to_end;
	/* was this a cut to end */
	ssize_t mark_begin_lineno;
	/* copy copy copy */
	ssize_t mark_begin_x;
	/* Another shadow variable */
	struct undo *next;
} undo;


typedef struct poshiststruct {
	char *filename;
	/* The file. */
	ssize_t lineno;
	/* Line number we left off on */
	ssize_t xno;
	/* x position in the file we left off on */
} poshiststruct;


typedef struct openfilestruct {
	char *filename;
	/* The current file's name. */

	filestruct *fileage;
	/* The current file's first line. */

	filestruct *filebot;
	/* The current file's last line. */

	filestruct *edittop;
	/* The current top of the edit window. */

	filestruct *current;
	/* The current file's current line. */

	size_t totsize;
	/* The current file's total number of characters. */

	size_t current_x;
	/* The current file's x-coordinate position. */

	size_t placewewant;
	/* The current file's place we want. */

	ssize_t current_y;
	/* The current file's y-coordinate position. */

	bool modified;
	/* Whether the current file has been modified. */

	bool mark_set;
	/* Whether the mark is on in the current file. */

	filestruct *mark_begin;
	/* The current file's beginning marked line, if any. */

	size_t mark_begin_x;
	/* The current file's beginning marked line's x-coordinate
	 * position, if any. */

	file_format fmt;
	/* The current file's format. */

	struct stat *current_stat;
	/* The current file's stat. */

	undo *undotop;
	/* Top of the undo list */

	undo *current_undo;
	/* The current (i.e. n ext) level of undo */

	undo_type last_action;

	const char *lock_filename;
	/* The path of the lockfile, if we created one */

	Syntax *syntax;
	/* The syntax class for this file, if any */

	ColorList colorstrings;
	/* The current file's associated colors. */

	struct openfilestruct *next;
	/* Next node. */

	struct openfilestruct *prev;
	/* Previous node. */
} openfilestruct;

typedef struct rcoption {
	std::string name;
	/* The name of the rcfile option. */
	long flag;
	/* The flag associated with it, if any. */
	bool overridable;
	/* Whether this option is allowed on a per-syntax basis (true) or globally only (false) */
} rcoption;

typedef struct sc {
	char *keystr;
	/* The shortcut key for a function, ASCII version */
	function_type type;
	/* What kind of function key is it for convenience later */
	int seq;
	/* The actual sequence to check on the the type is determined */
	int menu;
	/* What list does this apply to */
	void (*scfunc)(void);
	/* The function we're going to run */
	int toggle;
	/* If a toggle, what we're toggling */
	bool execute;
	/* Whether to execute the function in question or just return
	   so the sequence can be caught by the calling code */
} sc;

typedef struct subnfunc {
	void (*scfunc)(void);
	/* What function is this */
	int menus;
	/* In what menus does this function applu */
	const char *desc;
	/* The function's description, e.g. "Page Up". */
	const char *help;
	/* The help file entry text for this function. */
	bool blank_after;
	/* Whether there should be a blank line after the help entry
	 * text for this function. */
	bool viewok;
	/* Is this function allowed when in view mode? */
	long toggle;
	/* If this is a toggle, if nonzero what toggle to set */
} subnfunc;


/* Enumeration to be used in flags table. See FLAGBIT and FLAGOFF
 * definitions. */
enum {
	DONTUSE,
	CASE_SENSITIVE,
	CONST_UPDATE,
	NO_HELP,
	NOFOLLOW_SYMLINKS,
	SUSPEND,
	NO_WRAP,
	AUTOINDENT,
	VIEW_MODE,
	USE_REGEXP,
	TEMP_FILE,
	CUT_TO_END,
	BACKWARDS_SEARCH,
	MULTIBUFFER,
	SMOOTH_SCROLL,
	REBIND_DELETE,
	REBIND_KEYPAD,
	NO_CONVERT,
	BACKUP_FILE,
	INSECURE_BACKUP,
	NO_COLOR_SYNTAX,
	PRESERVE,
	HISTORYLOG,
	SMART_HOME,
	WHITESPACE_DISPLAY,
	MORE_SPACE,
	TABS_TO_SPACES,
	QUICK_BLANK,
	WORD_BOUNDS,
	NO_NEWLINES,
	BOLD_TEXT,
	QUIET,
	UNDOABLE,
	SOFTWRAP,
	POS_HISTORY,
	LOCKING
};

/* Flags for which menus in which a given function should be present */
#define MMAIN         (1<<0)
#define MWHEREIS      (1<<1)
#define MREPLACE      (1<<2)
#define MREPLACEWITH  (1<<3)
#define MGOTOLINE     (1<<4)
#define MWRITEFILE    (1<<5)
#define MINSERTFILE   (1<<6)
#define MEXTCMD       (1<<7)
#define MHELP         (1<<8)
#define MSPELL        (1<<9)
#define MBROWSER			(1<<10)
#define MWHEREISFILE  (1<<11)
#define MGOTODIR      (1<<12)
#define MYESNO        (1<<13)
#define MLINTER       (1<<14)
/* This is an abbreviation for all menus except Help and YesNo. */
#define MMOST         (MMAIN|MWHEREIS|MREPLACE|MREPLACEWITH|MGOTOLINE|MWRITEFILE|MINSERTFILE|MEXTCMD|MSPELL|MBROWSER|MWHEREISFILE|MGOTODIR|MLINTER)

/* Extra bits for the undo function */
#define UNdel_del		(1<<0)
#define UNdel_backspace	(1<<1)
#define UNsplit_madenew	(1<<2)

/* Since in ISO C you can't pass around function pointers anymore,
  let's make some integer macros for function names, and then I
  can go cut my wrists after writing the big switch statement
  that will necessitate. */

#define VIEW true
#define NOVIEW false

/* The maximum number of entries displayed in the main shortcut list. */
#define MAIN_VISIBLE (((COLS + 40) / 20) * 2)

/* The minimum editor window columns and rows required for pinot to work correctly. */
#define MIN_EDITOR_COLS 4
#define MIN_EDITOR_ROWS 1

/* The default number of characters from the end of the line where
 * wrapping occurs. */
#define CHARS_FROM_EOL 8

/* The default width of a tab in spaces. */
#define WIDTH_OF_TAB 8

/* The maximum number of search/replace history strings saved, not
 * counting the blank lines at their ends. */
#define MAX_SEARCH_HISTORY 100

/* The maximum number of bytes buffered at one time. */
#define MAX_BUF_SIZE 128

/* Some exit codes that we might want to check for. */
#define COMMAND_FAILED_PERMISSION_DENIED 126
#define COMMAND_FAILED_NOT_FOUND 127
