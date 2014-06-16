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
#include "OpenFile.h"
#include "cpputil.h"

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

#include "syntax.h"

/* The elements of the interface that can be colored differently. */
enum {
	TITLE_BAR,
	STATUS_BAR,
	KEY_COMBO,
	FUNCTION_TAG,
	NUMBER_OF_ELEMENTS
};

/* Enumeration to be used in flags table. See FLAGBIT and FLAGOFFSET definitions. */
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
#define UNdel_del         (1<<0)
#define UNdel_backspace   (1<<1)
#define UNsplit_completed (1<<2)
#define UNcut_cutline     (1<<3)

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
