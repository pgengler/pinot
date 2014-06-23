#pragma once

#include <string>

#include <sys/types.h>

#include "syntax.h"

/* Enumeration types. */
typedef enum {
	NIX_FILE, DOS_FILE, MAC_FILE
} FileFormat;

typedef enum {
	OVERWRITE, APPEND, PREPEND
} AppendType;

typedef enum {
	UPWARD, DOWNWARD
} ScrollDir;

typedef enum {
	CENTER, NONE
} UpdateType;

typedef enum {
	ADD, DEL, BACK, REPLACE, SPLIT_BEGIN, SPLIT_END, JOIN, CUT, CUT_EOF, PASTE, ENTER, INSERT, OTHER
} UndoType;

typedef enum {
	PROMPT_BLANK_STRING = -2, PROMPT_ABORTED, PROMPT_ENTER_PRESSED, PROMPT_OTHER_KEY
} PromptResult;

typedef enum {
	YESNO_PROMPT_ABORTED = -1, YESNO_PROMPT_NO, YESNO_PROMPT_YES, YESNO_PROMPT_ALL, YESNO_PROMPT_UNKNOWN = 255
} YesNoPromptResult;

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
	UndoType type;
	/* What type of undo was this */
	size_t begin;
	/* Where did this  action begin or end */
	char *strdata;
	/* String type data we will use for ccopying the affected line back */
	int xflags;
	/* Some flag data we need */

	/* Cut specific stuff we need */
	filestruct *cutbuffer;
	/* Copy of the cutbuffer */
	filestruct *cutbottom;
	/* Copy of cutbottom */
	bool mark_set;
	/* was the marker set when we cut */
	ssize_t mark_begin_lineno;
	/* copy copy copy */
	size_t mark_begin_x;
	/* Another shadow variable */
	struct undo *next;
} undo;


typedef struct poshiststruct {
	std::string filename;
	/* The file. */
	ssize_t lineno;
	/* Line number we left off on */
	ssize_t xno;
	/* x position in the file we left off on */
} poshiststruct;

typedef struct rcoption {
	std::string name;
	/* The name of the rcfile option. */
	long flag;
	/* The flag associated with it, if any. */
	bool overridable;
	/* Whether this option is allowed on a per-syntax basis (true) or globally only (false) */
} rcoption;

typedef struct sc {
	std::string keystr;
	/* The shortcut key for a function, ASCII version */
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
	/* In what menus does this function apply */
	std::string desc;
	/* The function's description, e.g. "Page Up". */
	std::string help;
	/* The help file entry text for this function. */
	bool blank_after;
	/* Whether there should be a blank line after the help entry
	 * text for this function. */
	bool viewok;
	/* Is this function allowed when in view mode? */
	long toggle;
	/* If this is a toggle, if nonzero what toggle to set */
} subnfunc;
