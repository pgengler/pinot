#pragma once

#include <string>

#include <sys/types.h>

#include "syntax.h"

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
