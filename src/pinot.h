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

#include <istream>
#include <string>
#include "macros.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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

/* If we aren't using ncurses with mouse support, turn the mouse support
 * off, as it's useless then. */
#ifndef NCURSES_MOUSE_VERSION
#define DISABLE_MOUSE 1
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
	struct poshiststruct *next;
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

typedef struct shortcut {
	const char *desc;
	/* The function's description, e.g. "Page Up". */
	const char *help;
	/* The help file entry text for this function. */
	bool blank_after;
	/* Whether there should be a blank line after the help entry
	 * text for this function. */
	/* Note: Key values that aren't used should be set to
	 * PINOT_NO_KEY. */
	int ctrlval;
	/* The special sentinel key or control key we want bound, if
	 * any. */
	int metaval;
	/* The meta key we want bound, if any. */
	int funcval;
	/* The function key we want bound, if any. */
	int miscval;
	/* The other meta key we want bound, if any. */
	bool viewok;
	/* Is this function allowed when in view mode? */
	void (*func)(void);
	/* The function to call when we get this key. */
	struct shortcut *next;
	/* Next shortcut. */
} shortcut;

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
	struct sc *next;
	/* Next in the list */
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
	struct subnfunc *next;
	/* next item in the list */
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
	USE_MOUSE,
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
	RESTRICTED,
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
#define MMAIN				(1<<0)
#define	MWHEREIS			(1<<1)
#define	MREPLACE			(1<<2)
#define	MREPLACE2			(1<<3)
#define	MGOTOLINE			(1<<4)
#define	MWRITEFILE			(1<<5)
#define	MINSERTFILE			(1<<6)
#define	MEXTCMD				(1<<7)
#define	MHELP				(1<<8)
#define	MSPELL				(1<<9)
#define	MBROWSER			(1<<10)
#define	MWHEREISFILE			(1<<11)
#define MGOTODIR			(1<<12)
#define MYESNO				(1<<13)
/* This really isnt all but close enough */
#define	MALL				(MMAIN|MWHEREIS|MREPLACE|MREPLACE2|MGOTOLINE|MWRITEFILE|MINSERTFILE|MEXTCMD|MSPELL|MBROWSER|MWHEREISFILE|MGOTODIR|MHELP)

/* Control key sequences.  Changing these would be very, very bad. */
#define PINOT_CONTROL_SPACE 0
#define PINOT_CONTROL_A 1
#define PINOT_CONTROL_B 2
#define PINOT_CONTROL_C 3
#define PINOT_CONTROL_D 4
#define PINOT_CONTROL_E 5
#define PINOT_CONTROL_F 6
#define PINOT_CONTROL_G 7
#define PINOT_CONTROL_H 8
#define PINOT_CONTROL_I 9
#define PINOT_CONTROL_J 10
#define PINOT_CONTROL_K 11
#define PINOT_CONTROL_L 12
#define PINOT_CONTROL_M 13
#define PINOT_CONTROL_N 14
#define PINOT_CONTROL_O 15
#define PINOT_CONTROL_P 16
#define PINOT_CONTROL_Q 17
#define PINOT_CONTROL_R 18
#define PINOT_CONTROL_S 19
#define PINOT_CONTROL_T 20
#define PINOT_CONTROL_U 21
#define PINOT_CONTROL_V 22
#define PINOT_CONTROL_W 23
#define PINOT_CONTROL_X 24
#define PINOT_CONTROL_Y 25
#define PINOT_CONTROL_Z 26
#define PINOT_CONTROL_3 27
#define PINOT_CONTROL_4 28
#define PINOT_CONTROL_5 29
#define PINOT_CONTROL_6 30
#define PINOT_CONTROL_7 31
#define PINOT_CONTROL_8 127

/* Meta key sequences. */
#define PINOT_META_SPACE ' '
#define PINOT_META_LPARENTHESIS '('
#define PINOT_META_RPARENTHESIS ')'
#define PINOT_META_PLUS '+'
#define PINOT_META_COMMA ','
#define PINOT_META_MINUS '-'
#define PINOT_META_PERIOD '.'
#define PINOT_META_SLASH '/'
#define PINOT_META_0 '0'
#define PINOT_META_6 '6'
#define PINOT_META_9 '9'
#define PINOT_META_LCARET '<'
#define PINOT_META_EQUALS '='
#define PINOT_META_RCARET '>'
#define PINOT_META_QUESTION '?'
#define PINOT_META_BACKSLASH '\\'
#define PINOT_META_RBRACKET ']'
#define PINOT_META_CARET '^'
#define PINOT_META_UNDERSCORE '_'
#define PINOT_META_A 'a'
#define PINOT_META_B 'b'
#define PINOT_META_C 'c'
#define PINOT_META_D 'd'
#define PINOT_META_E 'e'
#define PINOT_META_F 'f'
#define PINOT_META_G 'g'
#define PINOT_META_H 'h'
#define PINOT_META_I 'i'
#define PINOT_META_J 'j'
#define PINOT_META_K 'k'
#define PINOT_META_L 'l'
#define PINOT_META_M 'm'
#define PINOT_META_N 'n'
#define PINOT_META_O 'o'
#define PINOT_META_P 'p'
#define PINOT_META_Q 'q'
#define PINOT_META_R 'r'
#define PINOT_META_S 's'
#define PINOT_META_T 't'
#define PINOT_META_U 'u'
#define PINOT_META_V 'v'
#define PINOT_META_W 'w'
#define PINOT_META_X 'x'
#define PINOT_META_Y 'y'
#define PINOT_META_Z 'z'
#define PINOT_META_LCURLYBRACKET '{'
#define PINOT_META_PIPE '|'
#define PINOT_META_RCURLYBRACKET '}'

/* Some semi-changeable keybindings; don't play with these unless you're
 * sure you know what you're doing.  Assume ERR is defined as -1. */

/* No key at all. */
#define PINOT_NO_KEY			-2

/* Normal keys. */
#define PINOT_XON_KEY			PINOT_CONTROL_Q
#define PINOT_XOFF_KEY			PINOT_CONTROL_S
#define PINOT_CANCEL_KEY			PINOT_CONTROL_C
#define PINOT_EXIT_KEY			PINOT_CONTROL_X
#define PINOT_EXIT_FKEY			KEY_F(2)
#define PINOT_INSERTFILE_KEY		PINOT_CONTROL_R
#define PINOT_INSERTFILE_FKEY		KEY_F(5)
#define PINOT_TOOTHERINSERT_KEY		PINOT_CONTROL_X
#define PINOT_WRITEOUT_KEY		PINOT_CONTROL_O
#define PINOT_WRITEOUT_FKEY		KEY_F(3)
#define PINOT_GOTOLINE_KEY		PINOT_CONTROL_7
#define PINOT_GOTOLINE_FKEY		KEY_F(13)
#define PINOT_GOTOLINE_METAKEY		PINOT_META_G
#define PINOT_GOTODIR_KEY		PINOT_CONTROL_7
#define PINOT_GOTODIR_FKEY		KEY_F(13)
#define PINOT_GOTODIR_METAKEY		PINOT_META_G
#define PINOT_TOGOTOLINE_KEY		PINOT_CONTROL_T
#define PINOT_HELP_KEY			PINOT_CONTROL_G
#define PINOT_HELP_FKEY			KEY_F(1)
#define PINOT_WHEREIS_KEY		PINOT_CONTROL_W
#define PINOT_WHEREIS_FKEY		KEY_F(6)
#define PINOT_WHEREIS_NEXT_KEY		PINOT_META_W
#define PINOT_WHEREIS_NEXT_FKEY		KEY_F(16)
#define PINOT_TOOTHERWHEREIS_KEY		PINOT_CONTROL_T
#define PINOT_REGEXP_KEY			PINOT_META_R
#define PINOT_REPLACE_KEY		PINOT_CONTROL_4
#define PINOT_REPLACE_FKEY		KEY_F(14)
#define PINOT_REPLACE_METAKEY		PINOT_META_R
#define PINOT_TOOTHERSEARCH_KEY		PINOT_CONTROL_R
#define PINOT_PREVPAGE_KEY		PINOT_CONTROL_Y
#define PINOT_PREVPAGE_FKEY		KEY_F(7)
#define PINOT_NEXTPAGE_KEY		PINOT_CONTROL_V
#define PINOT_NEXTPAGE_FKEY		KEY_F(8)
#define PINOT_CUT_KEY			PINOT_CONTROL_K
#define PINOT_CUT_FKEY			KEY_F(9)
#define PINOT_COPY_KEY			PINOT_META_CARET
#define PINOT_COPY_METAKEY		PINOT_META_6
#define PINOT_UNCUT_KEY			PINOT_CONTROL_U
#define PINOT_UNCUT_FKEY			KEY_F(10)
#define PINOT_CURSORPOS_KEY		PINOT_CONTROL_C
#define PINOT_CURSORPOS_FKEY		KEY_F(11)
#define PINOT_SPELL_KEY			PINOT_CONTROL_T
#define PINOT_SPELL_FKEY			KEY_F(12)
#define PINOT_FIRSTLINE_KEY		PINOT_PREVPAGE_KEY
#define PINOT_FIRSTLINE_FKEY		PINOT_PREVPAGE_FKEY
#define PINOT_FIRSTLINE_METAKEY		PINOT_META_BACKSLASH
#define PINOT_FIRSTLINE_METAKEY2		PINOT_META_PIPE
#define PINOT_FIRSTFILE_KEY		PINOT_FIRSTLINE_KEY
#define PINOT_FIRSTFILE_FKEY		PINOT_FIRSTLINE_FKEY
#define PINOT_FIRSTFILE_METAKEY		PINOT_FIRSTLINE_METAKEY
#define PINOT_FIRSTFILE_METAKEY2		PINOT_FIRSTLINE_METAKEY2
#define PINOT_LASTLINE_KEY		PINOT_NEXTPAGE_KEY
#define PINOT_LASTLINE_FKEY		PINOT_NEXTPAGE_FKEY
#define PINOT_LASTLINE_METAKEY		PINOT_META_SLASH
#define PINOT_LASTLINE_METAKEY2		PINOT_META_QUESTION
#define PINOT_LASTFILE_KEY		PINOT_LASTLINE_KEY
#define PINOT_LASTFILE_FKEY		PINOT_LASTLINE_FKEY
#define PINOT_LASTFILE_METAKEY		PINOT_LASTLINE_METAKEY
#define PINOT_LASTFILE_METAKEY2		PINOT_LASTLINE_METAKEY2
#define PINOT_REFRESH_KEY		PINOT_CONTROL_L
#define PINOT_PREVLINE_KEY		PINOT_CONTROL_P
#define PINOT_NEXTLINE_KEY		PINOT_CONTROL_N
#define PINOT_FORWARD_KEY		PINOT_CONTROL_F
#define PINOT_BACK_KEY			PINOT_CONTROL_B
#define PINOT_MARK_KEY			PINOT_CONTROL_6
#define PINOT_MARK_METAKEY		PINOT_META_A
#define PINOT_MARK_FKEY			KEY_F(15)
#define PINOT_HOME_KEY			PINOT_CONTROL_A
#define PINOT_END_KEY			PINOT_CONTROL_E
#define PINOT_DELETE_KEY			PINOT_CONTROL_D
#define PINOT_BACKSPACE_KEY		PINOT_CONTROL_H
#define PINOT_TAB_KEY			PINOT_CONTROL_I
#define PINOT_INDENT_KEY			PINOT_META_RCURLYBRACKET
#define PINOT_UNINDENT_KEY		PINOT_META_LCURLYBRACKET
#define PINOT_SUSPEND_KEY		PINOT_CONTROL_Z
#define PINOT_ENTER_KEY			PINOT_CONTROL_M
#define PINOT_TOFILES_KEY		PINOT_CONTROL_T
#define PINOT_APPEND_KEY			PINOT_META_A
#define PINOT_PREPEND_KEY		PINOT_META_P
#define PINOT_PREVFILE_KEY		PINOT_META_LCARET
#define PINOT_PREVFILE_METAKEY		PINOT_META_COMMA
#define PINOT_NEXTFILE_KEY		PINOT_META_RCARET
#define PINOT_NEXTFILE_METAKEY		PINOT_META_PERIOD
#define PINOT_BRACKET_KEY		PINOT_META_RBRACKET
#define PINOT_NEXTWORD_KEY		PINOT_CONTROL_SPACE
#define PINOT_PREVWORD_KEY		PINOT_META_SPACE
#define PINOT_WORDCOUNT_KEY		PINOT_META_D
#define PINOT_SCROLLUP_KEY		PINOT_META_MINUS
#define PINOT_SCROLLDOWN_KEY		PINOT_META_PLUS
#define PINOT_SCROLLUP_METAKEY		PINOT_META_UNDERSCORE
#define PINOT_SCROLLDOWN_METAKEY		PINOT_META_EQUALS
#define PINOT_CUTTILLEND_METAKEY		PINOT_META_T
#define PINOT_PARABEGIN_KEY		PINOT_CONTROL_W
#define PINOT_PARABEGIN_METAKEY		PINOT_META_LPARENTHESIS
#define PINOT_PARABEGIN_METAKEY2		PINOT_META_9
#define PINOT_PARAEND_KEY		PINOT_CONTROL_O
#define PINOT_PARAEND_METAKEY		PINOT_META_RPARENTHESIS
#define PINOT_PARAEND_METAKEY2		PINOT_META_0
#define PINOT_VERBATIM_KEY		PINOT_META_V

/* No toggle at all. */
#define TOGGLE_NO_KEY			-2

/* Normal toggles. */
#define TOGGLE_NOHELP_KEY		PINOT_META_X
#define TOGGLE_CONST_KEY		PINOT_META_C
#define TOGGLE_MORESPACE_KEY		PINOT_META_O
#define TOGGLE_SMOOTH_KEY		PINOT_META_S
#define TOGGLE_WHITESPACE_KEY		PINOT_META_P
#define TOGGLE_SYNTAX_KEY		PINOT_META_Y
#define TOGGLE_SMARTHOME_KEY		PINOT_META_H
#define TOGGLE_AUTOINDENT_KEY		PINOT_META_I
#define TOGGLE_CUTTOEND_KEY		PINOT_META_K
#define TOGGLE_WRAP_KEY			PINOT_META_L
#define TOGGLE_TABSTOSPACES_KEY		PINOT_META_Q
#define TOGGLE_BACKUP_KEY		PINOT_META_B
#define TOGGLE_MULTIBUFFER_KEY		PINOT_META_F
#define TOGGLE_MOUSE_KEY		PINOT_META_M
#define TOGGLE_NOCONVERT_KEY		PINOT_META_N
#define TOGGLE_SUSPEND_KEY		PINOT_META_Z
#define TOGGLE_CASE_KEY			PINOT_META_C
#define TOGGLE_BACKWARDS_KEY		PINOT_META_B
#define TOGGLE_DOS_KEY			PINOT_META_D
#define TOGGLE_MAC_KEY			PINOT_META_M

/* Extra bits for the undo function */
#define UNdel_del		(1<<0)
#define UNdel_backspace	(1<<1)
#define UNsplit_madenew	(1<<2)

/* Since in ISO C you can't pass around function pointers anymore,
  let's make some integer macros for function names, and then I
  can go cut my wrists after writing the big switch statement
  that will necessitate. */

#define VIEW TRUE
#define NOVIEW FALSE

/* The maximum number of entries displayed in the main shortcut list. */
#define MAIN_VISIBLE 12

/* The minimum editor window columns and rows required for pinot to work
 * correctly. */
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

#include "EventQueue.h"
