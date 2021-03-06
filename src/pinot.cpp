/**************************************************************************
 *   pinot.c                                                              *
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

#include "proto.h"

#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <locale.h>
#include <time.h>
#include <langinfo.h>
#include <termios.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <sys/ioctl.h>

static bool no_rcfiles = false;
/* Should we ignore all rcfiles? */
static struct termios oldterm;
/* The user's original terminal settings. */
static struct sigaction act;
/* Used to set up all our fun signal handlers. */

/* Create a new filestruct node.  Note that we do not set prevnode->next
 * to the new line. */
filestruct *make_new_node(filestruct *prevnode)
{
	filestruct *newnode = new filestruct;

	newnode->data = NULL;
	newnode->prev = prevnode;
	newnode->next = NULL;
	newnode->lineno = (prevnode != NULL) ? prevnode->lineno + 1 : 1;

	return newnode;
}

/* Make a copy of a filestruct node. */
filestruct *copy_node(const filestruct *src)
{
	filestruct *dst;

	assert(src != NULL);

	dst = new filestruct;

	dst->data = mallocstrcpy(NULL, src->data);
	dst->next = src->next;
	dst->prev = src->prev;
	dst->lineno = src->lineno;

	return dst;
}

/* Splice a node into an existing filestruct. */
void splice_node(filestruct *begin, filestruct *newnode, filestruct *end)
{
	assert(newnode != NULL && begin != NULL);

	newnode->next = end;
	newnode->prev = begin;
	begin->next = newnode;
	if (end != NULL) {
		end->prev = newnode;
	}
}

/* Unlink a node from the rest of the filestruct. */
void unlink_node(const filestruct *fileptr)
{
	assert(fileptr != NULL);

	if (fileptr->prev != NULL) {
		fileptr->prev->next = fileptr->next;
	}
	if (fileptr->next != NULL) {
		fileptr->next->prev = fileptr->prev;
	}
}

/* Delete a node from the filestruct. */
void delete_node(filestruct *fileptr)
{
	assert(fileptr != NULL);

	free(fileptr->data);

	delete fileptr;
}

/* Duplicate a whole filestruct. */
filestruct *copy_filestruct(const filestruct *src)
{
	filestruct *head, *copy;

	assert(src != NULL);

	copy = copy_node(src);
	copy->prev = NULL;
	head = copy;
	src = src->next;

	while (src != NULL) {
		copy->next = copy_node(src);
		copy->next->prev = copy;
		copy = copy->next;

		src = src->next;
	}

	copy->next = NULL;

	return head;
}

/* Free a filestruct. */
void free_filestruct(filestruct *src)
{
	assert(src != NULL);

	while (src->next != NULL) {
		src = src->next;
		delete_node(src->prev);
	}

	delete_node(src);
}

/* Renumber all entries in a filestruct, starting with fileptr. */
void renumber(filestruct *fileptr)
{
	ssize_t line;

	if (fileptr == NULL) {
		return;
	}

	line = (fileptr->prev == NULL) ? 0 : fileptr->prev->lineno;

	assert(fileptr != fileptr->next);

	for (; fileptr != NULL; fileptr = fileptr->next) {
		fileptr->lineno = ++line;
	}
}

/* Partition a filestruct so that it begins at (top, top_x) and ends at
 * (bot, bot_x). */
partition *partition_filestruct(filestruct *top, size_t top_x, filestruct *bot, size_t bot_x)
{
	partition *p;

	assert(top != NULL && bot != NULL && openfile->fileage != NULL && openfile->filebot != NULL);

	/* Initialize the partition. */
	p = (partition *)nmalloc(sizeof(partition));

	/* If the top and bottom of the partition are different from the top
	 * and bottom of the filestruct, save the latter and then set them
	 * to top and bot. */
	if (top != openfile->fileage) {
		p->fileage = openfile->fileage;
		openfile->fileage = top;
	} else {
		p->fileage = NULL;
	}
	if (bot != openfile->filebot) {
		p->filebot = openfile->filebot;
		openfile->filebot = bot;
	} else {
		p->filebot = NULL;
	}

	/* Save the line above the top of the partition, detach the top of
	 * the partition from it, and save the text before top_x in
	 * top_data. */
	p->top_prev = top->prev;
	top->prev = NULL;
	p->top_data = mallocstrncpy(NULL, top->data, top_x + 1);
	p->top_data[top_x] = '\0';

	/* Save the line below the bottom of the partition, detach the
	 * bottom of the partition from it, and save the text after bot_x in
	 * bot_data. */
	p->bot_next = bot->next;
	bot->next = NULL;
	p->bot_data = mallocstrcpy(NULL, bot->data + bot_x);

	/* Remove all text after bot_x at the bottom of the partition. */
	null_at(&bot->data, bot_x);

	/* Remove all text before top_x at the top of the partition. */
	charmove(top->data, top->data + top_x, strlen(top->data) - top_x + 1);
	align(&top->data);

	/* Return the partition. */
	return p;
}

/* Unpartition a filestruct so that it begins at (fileage, 0) and ends
 * at (filebot, strlen(filebot->data)) again. */
void unpartition_filestruct(partition **p)
{
	char *tmp;

	assert(p != NULL && openfile->fileage != NULL && openfile->filebot != NULL);

	/* Reattach the line above the top of the partition, and restore the
	 * text before top_x from top_data.  Free top_data when we're done
	 * with it. */
	tmp = mallocstrcpy(NULL, openfile->fileage->data);
	openfile->fileage->prev = (*p)->top_prev;
	if (openfile->fileage->prev != NULL) {
		openfile->fileage->prev->next = openfile->fileage;
	}
	openfile->fileage->data = charealloc(openfile->fileage->data, strlen((*p)->top_data) + strlen(openfile->fileage->data) + 1);
	strcpy(openfile->fileage->data, (*p)->top_data);
	free((*p)->top_data);
	strcat(openfile->fileage->data, tmp);
	free(tmp);

	/* Reattach the line below the bottom of the partition, and restore
	 * the text after bot_x from bot_data.  Free bot_data when we're
	 * done with it. */
	openfile->filebot->next = (*p)->bot_next;
	if (openfile->filebot->next != NULL) {
		openfile->filebot->next->prev = openfile->filebot;
	}
	openfile->filebot->data = charealloc(openfile->filebot->data, strlen(openfile->filebot->data) + strlen((*p)->bot_data) + 1);
	strcat(openfile->filebot->data, (*p)->bot_data);
	free((*p)->bot_data);

	/* Restore the top and bottom of the filestruct, if they were
	 * different from the top and bottom of the partition. */
	if ((*p)->fileage != NULL) {
		openfile->fileage = (*p)->fileage;
	}
	if ((*p)->filebot != NULL) {
		openfile->filebot = (*p)->filebot;
	}

	/* Uninitialize the partition. */
	free(*p);
	*p = NULL;
}

/* Move all the text between (top, top_x) and (bot, bot_x) in the
 * current filestruct to a filestruct beginning with file_top and ending
 * with file_bot.  If no text is between (top, top_x) and (bot, bot_x),
 * don't do anything. */
void move_to_filestruct(filestruct **file_top, filestruct **file_bot, filestruct *top, size_t top_x, filestruct *bot, size_t bot_x)
{
	filestruct *top_save;
	bool edittop_inside;
	bool mark_inside = false;
	bool same_line = false;

	assert(file_top != NULL && file_bot != NULL && top != NULL && bot != NULL);

	/* If (top, top_x)-(bot, bot_x) doesn't cover any text, get out. */
	if (top == bot && top_x == bot_x) {
		return;
	}

	/* Partition the filestruct so that it contains only the text from
	 * (top, top_x) to (bot, bot_x), keep track of whether the top of
	 * the edit window is inside the partition, and keep track of
	 * whether the mark begins inside the partition. */
	filepart = partition_filestruct(top, top_x, bot, bot_x);
	edittop_inside = (openfile->edittop->lineno >= openfile->fileage->lineno && openfile->edittop->lineno <= openfile->filebot->lineno);
	if (openfile->mark_set) {
		mark_inside = (openfile->mark_begin->lineno >=
		               openfile->fileage->lineno &&
		               openfile->mark_begin->lineno <=
		               openfile->filebot->lineno &&
		               (openfile->mark_begin != openfile->fileage ||
		                openfile->mark_begin_x >= top_x) &&
		               (openfile->mark_begin != openfile->filebot ||
		                openfile->mark_begin_x <= bot_x));
		same_line = (openfile->mark_begin == openfile->fileage);
	}

	/* Get the number of characters in the text, and subtract it from
	 * totsize. */
	openfile->totsize -= get_totsize(top, bot);

	if (*file_top == NULL) {
		/* If file_top is empty, just move all the text directly into
		 * it.  This is equivalent to tacking the text in top onto the
		 * (lack of) text at the end of file_top. */
		*file_top = openfile->fileage;
		*file_bot = openfile->filebot;

		/* Renumber starting with file_top. */
		renumber(*file_top);
	} else {
		filestruct *file_bot_save = *file_bot;

		/* Otherwise, tack the text in top onto the text at the end of
		 * file_bot. */
		(*file_bot)->data = charealloc((*file_bot)->data, strlen((*file_bot)->data) + strlen(openfile->fileage->data) + 1);
		strcat((*file_bot)->data, openfile->fileage->data);

		/* Attach the line after top to the line after file_bot.  Then,
		 * if there's more than one line after top, move file_bot down
		 * to bot. */
		(*file_bot)->next = openfile->fileage->next;
		if ((*file_bot)->next != NULL) {
			(*file_bot)->next->prev = *file_bot;
			*file_bot = openfile->filebot;
		}

		openfile->fileage->next = NULL;
		free_filestruct(openfile->fileage);

		/* Renumber starting with the line after the original file_bot. */
		renumber(file_bot_save->next);
	}

	/* Since the text has now been saved, remove it from the filestruct. */
	openfile->fileage = new filestruct;
	openfile->fileage->data = mallocstrcpy(NULL, "");
	openfile->filebot = openfile->fileage;

	/* Restore the current line and cursor position.  If the mark begins
	 * inside the partition, set the beginning of the mark to where the
	 * saved text used to start. */
	openfile->current = openfile->fileage;
	openfile->current_x = top_x;
	if (mark_inside) {
		openfile->mark_begin = openfile->current;
		openfile->mark_begin_x = openfile->current_x;
	} else if (same_line) {
		/* update the content of this partially cut line */
		openfile->mark_begin = openfile->current;
	}

	top_save = openfile->fileage;

	/* Unpartition the filestruct so that it contains all the text
	 * again, minus the saved text. */
	unpartition_filestruct(&filepart);

	/* If the top of the edit window was inside the old partition, put
	 * it in range of current. */
	if (edittop_inside) {
		edit_update(NONE);
	}

	/* Renumber starting with the beginning line of the old partition. */
	renumber(top_save);

	/* If the NO_NEWLINES flag isn't set, and the text doesn't end with
	 * a magicline, add a new magicline. */
	if (!ISSET(NO_NEWLINES) && openfile->filebot->data[0] != '\0') {
		new_magicline();
	}
}

/* Copy all the text from the given filestruct to the current filestruct at the current cursor position. */
void copy_from_filestruct(filestruct *some_buffer)
{
	filestruct *top_save;
	size_t current_x_save = openfile->current_x;
	bool edittop_inside;
	bool right_side_up = false, single_line = false;

	assert(some_buffer != NULL);

	/* Keep track of whether the mark begins inside the partition and
	 * will need adjustment. */
	if (openfile->mark_set) {
		filestruct *top, *bot;
		size_t top_x, bot_x;

		mark_order((const filestruct **)&top, &top_x, (const filestruct **)&bot, &bot_x, &right_side_up);

		single_line = (top == bot);
	}

	/* Partition the filestruct so that it contains no text, and keep
	 * track of whether the top of the edit window is inside the
	 * partition. */
	filepart = partition_filestruct(openfile->current, openfile->current_x, openfile->current, openfile->current_x);
	edittop_inside = (openfile->edittop == openfile->fileage);

	/* Put the top and bottom of the current filestruct at the top and bottom of of a copy of the passed buffer */
	free_filestruct(openfile->fileage);
	openfile->fileage = copy_filestruct(some_buffer);
	openfile->filebot = openfile->fileage;
	while (openfile->filebot->next != NULL) {
		openfile->filebot = openfile->filebot->next;
	}

	/* Restore the current line and cursor position.  If the mark begins
	 * inside the partition, adjust the mark coordinates to compensate
	 * for the change in the current line. */
	openfile->current = openfile->filebot;
	openfile->current_x = strlen(openfile->filebot->data);
	if (openfile->fileage == openfile->filebot) {
		if (openfile->mark_set) {
			openfile->mark_begin = openfile->current;
			if (!right_side_up) {
				openfile->mark_begin_x += openfile->current_x;
			}
		}
		openfile->current_x += current_x_save;
	} else if (openfile->mark_set) {
		if (right_side_up) {
			if (single_line) {
				/* get the new data, stuff was inserted on mark line */
				openfile->mark_begin = openfile->fileage;
				/* the x is okay, it did not move */
			}
		} else {
			if (single_line) {
				openfile->mark_begin = openfile->current;
				openfile->mark_begin_x -= current_x_save;
			} else {
				openfile->mark_begin_x -= openfile->current_x;
			}
		}
	}

	/* Get the number of characters in the copied text, and add it to
	 * totsize. */
	openfile->totsize += get_totsize(openfile->fileage, openfile->filebot);

	/* Update the current y-coordinate to account for the number of
	 * lines the copied text has, less one since the first line will be
	 * tacked onto the current line. */
	openfile->current_y += openfile->filebot->lineno - 1;

	top_save = openfile->fileage;

	/* If the top of the edit window is inside the partition, set it to
	 * where the copied text now starts. */
	if (edittop_inside) {
		openfile->edittop = openfile->fileage;
	}

	/* Unpartition the filestruct so that it contains all the text
	 * again, plus the copied text. */
	unpartition_filestruct(&filepart);

	/* Renumber starting with the beginning line of the old
	 * partition. */
	renumber(top_save);

	/* If the NO_NEWLINES flag isn't set, and the text doesn't end with
	 * a magicline, add a new magicline. */
	if (!ISSET(NO_NEWLINES) && openfile->filebot->data[0] != '\0') {
		new_magicline();
	}
}

/* Display a warning about a key disabled in view mode. */
void print_view_warning(void)
{
	statusbar(_("Key invalid in view mode"));
}

/* Make pinot exit gracefully. */
void finish(void)
{
	/* Blank the statusbar (and shortcut list, if applicable), and move
	 * the cursor to the last line of the screen. */
	if (!ISSET(NO_HELP)) {
		blank_bottombars();
	} else {
		blank_statusbar();
	}
	wrefresh(bottomwin);
	endwin();

	/* Restore the old terminal settings. */
	tcsetattr(0, TCSANOW, &oldterm);

	if (!no_rcfiles && ISSET(HISTORYLOG)) {
		save_history();
	}
	if (!no_rcfiles && ISSET(POS_HISTORY)) {
		update_poshistory(openfile->filename, openfile->current->lineno, xplustabs()+1);
		save_poshistory();
	}

#ifdef DEBUG
	thanks_for_all_the_fish();
#endif

	/* Get out. */
	exit(0);
}

/* Make pinot die gracefully. */
void die(const char *msg, ...)
{
	va_list ap;

	endwin();

	/* Restore the old terminal settings. */
	tcsetattr(0, TCSANOW, &oldterm);

	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);

	/* Save the current file buffer if it's been modified. */
	if (openfile->modified) {
		/* If we've partitioned the filestruct, unpartition it now. */
		if (filepart != NULL) {
			unpartition_filestruct(&filepart);
		}

		die_save_file(openfile->filename, openfile->current_stat);
	}

	/* Save all of the other modified file buffers, if any. */
	if (openfile != openfiles.end()) {
		for (auto file : openfiles) {
			/* Save the current file buffer if it's been modified. */
			if (file.modified) {
				die_save_file(file.filename, file.current_stat);
			}
		}
	}

	/* Get out. */
	exit(1);
}

/* Save the current file under the name spacified in die_filename, which
 * is modified to be unique if necessary. */
void die_save_file(std::string die_filename, struct stat *die_stat)
{
	bool failed = true;

	/* If we can't save, we have really bad problems, but we might as well try. */
	if (die_filename == "") {
		die_filename = "pinot";
	}

	std::string retval = get_next_filename(die_filename, ".save");
	if (retval != "") {
		failed = !write_file(retval.c_str(), NULL, true, OVERWRITE, true);
	}

	if (!failed) {
		fprintf(stderr, _("\nBuffer written to %s\n"), retval.c_str());
	} else if (retval[0] != '\0')
		fprintf(stderr, _("\nBuffer not written to %s: %s\n"), retval.c_str(), strerror(errno));
	else
		fprintf(stderr, _("\nBuffer not written: %s\n"), _("Too many backup files?"));

	/* Try and chmod/chown the save file to the values of the original file, but
	   dont worry if it fails because we're supposed to be bailing as fast
	   as possible. */
	if (die_stat) {
		int shush;
		shush = chmod(retval, die_stat->st_mode);
		shush = chown(retval, die_stat->st_uid, die_stat->st_gid);
		UNUSED_VAR(shush);
	}
}

void keyboard_init(void)
{
	keyboard = new Keyboard();
}

/* Initialize the three window portions pinot uses. */
void window_init(void)
{
	/* If the screen height is too small, get out. */
	editwinrows = LINES - 5 + more_space() + no_help();
	if (COLS < MIN_EDITOR_COLS || editwinrows < MIN_EDITOR_ROWS) {
		die(_("Window size is too small for pinot...\n"));
	}

	/* Set up fill, based on the screen width. */
	fill = wrap_at;
	if (fill <= 0) {
		fill += COLS;
	}
	if (fill < 0) {
		fill = 0;
	}

	if (topwin != NULL) {
		delwin(topwin);
	}
	if (edit != NULL) {
		delwin(edit);
	}
	if (bottomwin != NULL) {
		delwin(bottomwin);
	}

	/* Set up the windows. */
	topwin = newwin(2 - more_space(), COLS, 0, 0);
	edit = newwin(editwinrows, COLS, 2 - more_space(), 0);
	bottomwin = newwin(3 - no_help(), COLS, editwinrows + (2 - more_space()), 0);

	/* Turn the keypad on for the windows, if necessary. */
	if (!ISSET(REBIND_KEYPAD)) {
		keypad(topwin, true);
		keypad(edit, true);
		keypad(bottomwin, true);
	}
}

#ifdef HAVE_GETOPT_LONG
#define print_opt(shortflag, longflag, desc) print_opt_full(shortflag, longflag, desc)
#else
#define print_opt(shortflag, longflag, desc) print_opt_full(shortflag, desc)
#endif

/* Print one usage string to the screen.  This cuts down on duplicate
 * strings to translate, and leaves out the parts that shouldn't be
 * translatable (i.e. the flag names). */
void print_opt_full(const char *shortflag
#ifdef HAVE_GETOPT_LONG
                    , const char *longflag
#endif
                    , const char *desc)
{
	printf(" %s\t", shortflag);
	if (strlenpt(shortflag) < 8) {
		printf("\t");
	}

#ifdef HAVE_GETOPT_LONG
	printf("%s\t", longflag);
	if (strlenpt(longflag) < 8) {
		printf("\t\t");
	} else if (strlenpt(longflag) < 16) {
		printf("\t");
	}
#endif

	if (desc != NULL) {
		printf("%s", _(desc));
	}
	printf("\n");
}

/* Explain how to properly use pinot and its command line options. */
void usage(void)
{
	printf(_("Usage: pinot [OPTIONS] [[+LINE,COLUMN] FILE]...\n\n"));
	printf(
#ifdef HAVE_GETOPT_LONG
	    _("Option\t\tGNU long option\t\tMeaning\n")
#else
	    _("Option\t\tMeaning\n")
#endif
	);
	print_opt("-h, -?", "--help", N_("Show this message"));
	print_opt(_("+LINE,COLUMN"), "", N_("Start at line LINE, column COLUMN"));
	print_opt("-A", "--smarthome", N_("Enable smart home key"));
	print_opt("-B", "--backup", N_("Save backups of existing files"));
	print_opt(_("-C <dir>"), _("--backupdir=<dir>"), N_("Directory for saving unique backup files"));
	print_opt("-D", "--boldtext", N_("Use bold instead of reverse video text"));
	print_opt("-E", "--tabstospaces", N_("Convert typed tabs to spaces"));
	print_opt("-F", "--multibuffer", N_("Read a file into a new buffer by default"));
	print_opt("-G", "--locking", N_("Use (vim-style) lock files"));
	print_opt("-H", "--historylog", N_("Log & read search/replace string history"));
	print_opt("-I", "--ignorercfiles", N_("Don't look at pinotorc files"));
	print_opt("-K", "--rebindkeypad", N_("Fix numeric keypad key confusion problem"));
	print_opt("-L", "--nonewlines", N_("Don't add newlines to the ends of files"));
	print_opt("-N", "--noconvert", N_("Don't convert files from DOS/Mac format"));
	print_opt("-O", "--morespace", N_("Use one more line for editing"));
	print_opt("-P", "--poslog", N_("Log & read location of cursor position"));
	print_opt("-S", "--smooth", N_("Scroll by line instead of half-screen"));
	print_opt(_("-T <#cols>"), _("--tabsize=<#cols>"), N_("Set width of a tab to #cols columns"));
	print_opt("-U", "--quickblank", N_("Do quick statusbar blanking"));
	print_opt("-V", "--version", N_("Print version information and exit"));
	print_opt("-W", "--wordbounds", N_("Detect word boundaries more accurately"));
	print_opt(_("-Y <str>"), _("--syntax=<str>"), N_("Syntax definition to use for coloring"));
	print_opt("-c", "--const", N_("Constantly show cursor position"));
	print_opt("-i", "--autoindent", N_("Automatically indent new lines"));
	print_opt("-k", "--cut", N_("Cut from cursor to end of line"));
	print_opt("-l", "--nofollow", N_("Don't follow symbolic links, overwrite"));
	print_opt("-p", "--preserve", N_("Preserve XON (^Q) and XOFF (^S) keys"));
	print_opt("-q", "--quiet", N_("Silently ignore startup issues like rc file errors"));
	print_opt(_("-r <#cols>"), _("--fill=<#cols>"), N_("Set wrapping point at column #cols"));
	print_opt(_("-s <prog>"), _("--speller=<prog>"), N_("Enable alternate speller"));
	print_opt("-t", "--tempfile", N_("Auto save on exit, don't prompt"));

	print_opt("-v", "--view", N_("View mode (read-only)"));
	print_opt("-w", "--nowrap", N_("Don't wrap long lines"));
	print_opt("-x", "--nohelp", N_("Don't show the two help lines"));
	print_opt("-z", "--suspend", N_("Enable suspension"));
	print_opt("-$", "--softwrap", N_("Enable soft line wrapping"));
}

/* Display the current version of pinot, the date and time it was
 * compiled, contact information for it, and the configuration options
 * it was compiled with. */
void version(void)
{
	printf(_(" pinot version %s (compiled %s, %s)\n"), VERSION, __TIME__, __DATE__);
	printf(" (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007,\n");
	printf(" 2008, 2009 Free Software Foundation, Inc.\n");
	printf( _(" Email: phil@pgengler.net	Web: http://github.com/pgengler"));
	printf(_("\n Compiled options:"));

#ifndef ENABLE_NLS
	printf(" --disable-nls");
#endif
#ifdef DISABLE_ROOTWRAPPING
	printf(" --disable-wrapping-as-root");
#endif
#ifdef DEBUG
	printf(" --enable-debug");
#endif
#ifdef USE_SLANG
	printf(" --with-slang");
#endif
	printf("\n");
}

/* Return 1 if the MORE_SPACE flag is set, and 0 otherwise.  This is
 * used to calculate the sizes and Y coordinates of the subwindows. */
int more_space(void)
{
	return ISSET(MORE_SPACE) ? 1 : 0;
}

/* Return 2 if the NO_HELP flag is set, and 0 otherwise.  This is used
 * to calculate the sizes and Y coordinates of the subwindows, because
 * having NO_HELP adds two lines to the edit window. */
int no_help(void)
{
	return ISSET(NO_HELP) ? 2 : 0;
}

/* Indicate a disabled function on the statusbar. */
void pinot_disabled_msg(void)
{
	statusbar(_("Sorry, support for this function has been disabled"));
}

/* If the current file buffer has been modified, and the TEMP_FILE flag
 * isn't set, ask whether or not to save the file buffer.  If the
 * TEMP_FILE flag is set, save it unconditionally.  Then, if more than
 * one file buffer is open, close the current file buffer and switch to
 * the next one.  If only one file buffer is open, exit from pinot. */
void do_exit(void)
{
	int i;

	if (!openfile->modified) {
		/* If the file hasn't been modified, pretend the user chose not to save. */
		i = 0;
	}	else if (ISSET(TEMP_FILE)) {
		/* If the TEMP_FILE flag is set, pretend the user chose to save. */
		i = 1;
	} else {
		/* Otherwise, ask the user whether or not to save. */
		i = do_yesno_prompt(false, _("Save modified buffer (ANSWERING \"No\" WILL DESTROY CHANGES) ? "));
	}

#ifdef DEBUG
	dump_filestruct(openfile->fileage);
#endif

	/* If the user chose not to save, or if the user chose to save and
	 * the save succeeded, we're ready to exit. */
	if (i == 0 || (i == 1 && do_writeout(true))) {
		if (ISSET(LOCKING) && openfile->lock_filename != "") {
			delete_lockfile(openfile->lock_filename);
		}
		/* Exit only if there are no more open file buffers. */
		if (!close_buffer(false)) {
			finish();
		}
		/* If the user canceled, we go on. */
	} else if (i != 1) {
		statusbar(_("Cancelled"));
	}

	shortcut_init();
	display_main_list();
}

/* Another placeholder for function mapping */
void do_cancel(void)
{
	;
}

static struct sigaction pager_oldaction, pager_newaction;  /* Original and temporary handlers for SIGINT. */
static bool pager_sig_failed = false; /* Did sigaction() fail without changing the signal handlers? */
static bool pager_input_aborted = false; /* Did someone invoke the pager and abort it via ^C? */


/* Things which need to be run regardless of whether
   we finished the stdin pipe correctly or not */
void finish_stdin_pager(void)
{
	FILE *f;
	int ttystdin;

	/* Read whatever we did get from stdin */
	f = fopen("/dev/stdin", "rb");
	if (f == NULL) {
		nperror("fopen");
	}

	read_file(f, 0, "stdin", true, false);
	ttystdin = open("/dev/tty", O_RDONLY);
	if (!ttystdin) {
		die(_("Couldn't reopen stdin from keyboard, sorry\n"));
	}

	dup2(ttystdin,0);
	close(ttystdin);
	if (!pager_input_aborted) {
		tcgetattr(0, &oldterm);
	}
	if (!pager_sig_failed && sigaction(SIGINT, &pager_oldaction, NULL) == -1) {
		nperror("sigaction");
	}
	terminal_init();
	doupdate();
}


/* Cancel reading from stdin like a pager */
void cancel_stdin_pager(int signal)
{
	UNUSED_VAR(signal);
	pager_input_aborted = true;
}

/* Let pinot read stdin for the first file at least */
void stdin_pager(void)
{
	endwin();
	if (!pager_input_aborted) {
		tcsetattr(0, TCSANOW, &oldterm);
	}
	fprintf(stderr, _("Reading from stdin, ^C to abort\n"));

	/* Enable interpretation of the special control keys so that we get
	 * SIGINT when Ctrl-C is pressed. */
	enable_signals();

	/* Set things up so that Ctrl-C will cancel the new process. */
	if (sigaction(SIGINT, NULL, &pager_newaction) == -1) {
		pager_sig_failed = true;
		nperror("sigaction");
	} else {
		pager_newaction.sa_handler = cancel_stdin_pager;
		if (sigaction(SIGINT, &pager_newaction, &pager_oldaction) == -1) {
			pager_sig_failed = true;
			nperror("sigaction");
		}
	}

	open_buffer("", false);
	finish_stdin_pager();
}



/* Initialize the signal handlers. */
void signal_init(void)
{
	/* Trap SIGINT and SIGQUIT because we want them to do useful
	 * things. */
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = SIG_IGN;
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGQUIT, &act, NULL);

	/* Trap SIGHUP and SIGTERM because we want to write the file out. */
	act.sa_handler = handle_hupterm;
	sigaction(SIGHUP, &act, NULL);
	sigaction(SIGTERM, &act, NULL);

	/* Trap SIGWINCH because we want to handle window resizes. */
	act.sa_handler = handle_sigwinch;
	sigaction(SIGWINCH, &act, NULL);
	allow_pending_sigwinch(false);

	/* Trap normal suspend (^Z) so we can handle it ourselves. */
	if (!ISSET(SUSPEND)) {
		act.sa_handler = SIG_IGN;
		sigaction(SIGTSTP, &act, NULL);
	} else {
		/* Block all other signals in the suspend and continue handlers.
		 * If we don't do this, other stuff interrupts them! */
		sigfillset(&act.sa_mask);

		act.sa_handler = do_suspend;
		sigaction(SIGTSTP, &act, NULL);

		act.sa_handler = do_continue;
		sigaction(SIGCONT, &act, NULL);
	}
}

/* Handler for SIGHUP (hangup) and SIGTERM (terminate). */
void handle_hupterm(int signal)
{
	UNUSED_VAR(signal);
	die(_("Received SIGHUP or SIGTERM\n"));
}

/* Handler for SIGTSTP (suspend). */
void do_suspend(int signal)
{
	UNUSED_VAR(signal);
	/* Move the cursor to the last line of the screen. */
	move(LINES - 1, 0);
	endwin();

	/* Display our helpful message. */
	printf(_("Use \"fg\" to return to pinot.\n"));
	fflush(stdout);

	/* Restore the old terminal settings. */
	tcsetattr(0, TCSANOW, &oldterm);

	/* Trap SIGHUP and SIGTERM so we can properly deal with them while
	 * suspended. */
	act.sa_handler = handle_hupterm;
	sigaction(SIGHUP, &act, NULL);
	sigaction(SIGTERM, &act, NULL);

	/* Do what mutt does: send ourselves a SIGSTOP. */
	kill(0, SIGSTOP);
}

/* the subnfunc version */
void do_suspend_void(void)
{
	if (ISSET(SUSPEND)) {
		do_suspend(0);
	}
}

/* Handler for SIGCONT (continue after suspend). */
void do_continue(int signal)
{
	UNUSED_VAR(signal);
	/* Perhaps the user resized the window while we slept.  Handle it,
	 * and restore the terminal to its previous state and update the
	 * screen in the process. */
	handle_sigwinch(0);
}

/* Handler for SIGWINCH (window size change). */
void handle_sigwinch(int signal)
{
	UNUSED_VAR(signal);
	const char *tty = ttyname(0);
	int fd, result = 0;
	struct winsize win;

	if (tty == NULL) {
		return;
	}
	fd = open(tty, O_RDWR);
	if (fd == -1) {
		return;
	}
	result = ioctl(fd, TIOCGWINSZ, &win);
	close(fd);
	if (result == -1) {
		return;
	}

	/* We could check whether the COLS or LINES changed, and return
	 * otherwise.  However, COLS and LINES are curses global variables,
	 * and in some cases curses has already updated them.  But not in
	 * all cases.  Argh. */
#ifdef REDEFINING_MACROS_OK
	COLS = win.ws_col;
	LINES = win.ws_row;
#endif

	/* If we've partitioned the filestruct, unpartition it now. */
	if (filepart != NULL) {
		unpartition_filestruct(&filepart);
	}

#ifdef USE_SLANG
	/* Slang curses emulation brain damage, part 1: If we just do what
	 * curses does here, it'll only work properly if the resize made the
	 * window smaller.  Do what mutt does: Leave and immediately reenter
	 * Slang screen management mode. */
	SLsmg_reset_smg();
	SLsmg_init_smg();
#else
	/* Do the equivalent of what Minimum Profit does: Leave and
	 * immediately reenter curses mode. */
	endwin();
	doupdate();
#endif

	/* Restore the terminal to its previous state. */
	terminal_init();

	/* Turn the cursor back on for sure. */
	curs_set(1);

	/* Do the equivalent of what both mutt and Minimum Profit do:
	 * Reinitialize all the windows based on the new screen
	 * dimensions. */
	window_init();

	/* Redraw the contents of the windows that need it. */
	blank_statusbar();
	wnoutrefresh(bottomwin);
	currmenu = MMAIN;
	total_refresh();

	/* Jump back to main() */
	siglongjmp(jump_buf, 1);
}

/* If allow is true, block any SIGWINCH signals that we get, so that we
 * can deal with them later.  If allow is false, unblock any SIGWINCH
 * signals that we have, so that we can deal with them now. */
void allow_pending_sigwinch(bool allow)
{
	sigset_t winch;
	sigemptyset(&winch);
	sigaddset(&winch, SIGWINCH);
	sigprocmask(allow ? SIG_UNBLOCK : SIG_BLOCK, &winch, NULL);
}

/* Handle the global toggle specified in which. */
void do_toggle(int flag)
{
	bool enabled;
	const char *desc;

	TOGGLE(flag);

	switch (flag) {
	case MORE_SPACE:
	case NO_HELP:
		window_init();
		total_refresh();
		break;
	case SUSPEND:
		signal_init();
		break;
	case WHITESPACE_DISPLAY:
		titlebar(NULL);
		edit_refresh();
		break;
	case NO_COLOR_SYNTAX:
	case SOFTWRAP:
		edit_refresh();
		break;
	}

	enabled = ISSET(flag);

	if (flag ==  NO_HELP || flag == NO_COLOR_SYNTAX || flag == NO_WRAP) {
		enabled = !enabled;
	}

	desc = _(flagtostr(flag));
	statusbar("%s %s", desc, enabled ? _("enabled") : _("disabled"));
}

/* Bleh */
void do_toggle_void(void)
{
	;
}

/* Disable extended input and output processing in our terminal
 * settings. */
void disable_extended_io(void)
{
	struct termios term;

	tcgetattr(0, &term);
	term.c_lflag &= ~IEXTEN;
	term.c_oflag &= ~OPOST;
	tcsetattr(0, TCSANOW, &term);
}

/* Disable interpretation of the special control keys in our terminal
 * settings. */
void disable_signals(void)
{
	struct termios term;

	tcgetattr(0, &term);
	term.c_lflag &= ~ISIG;
	tcsetattr(0, TCSANOW, &term);
}

/* Enable interpretation of the special control keys in our terminal
 * settings. */
void enable_signals(void)
{
	struct termios term;

	tcgetattr(0, &term);
	term.c_lflag |= ISIG;
	tcsetattr(0, TCSANOW, &term);
}

/* Disable interpretation of the flow control characters in our terminal
 * settings. */
void disable_flow_control(void)
{
	struct termios term;

	tcgetattr(0, &term);
	term.c_iflag &= ~IXON;
	tcsetattr(0, TCSANOW, &term);
}

/* Enable interpretation of the flow control characters in our terminal
 * settings. */
void enable_flow_control(void)
{
	struct termios term;

	tcgetattr(0, &term);
	term.c_iflag |= IXON;
	tcsetattr(0, TCSANOW, &term);
}

/* Set up the terminal state.  Put the terminal in raw mode (read one
 * character at a time, disable the special control keys, and disable
 * the flow control characters), disable translation of carriage return
 * (^M) into newline (^J) so that we can tell the difference between the
 * Enter key and Ctrl-J, and disable echoing of characters as they're
 * typed.  Finally, disable extended input and output processing, and,
 * if we're not in preserve mode, reenable interpretation of the flow
 * control characters. */
void terminal_init(void)
{
#ifdef USE_SLANG
	/* Slang curses emulation brain damage, part 2: Slang doesn't
	 * implement raw(), nonl(), or noecho() properly, so there's no way
	 * to properly reinitialize the terminal using them.  We have to
	 * disable the special control keys and interpretation of the flow
	 * control characters using termios, save the terminal state after
	 * the first call, and restore it on subsequent calls. */
	static struct termios newterm;
	static bool newterm_set = false;

	if (!newterm_set) {
#endif

		raw();
		nonl();
		noecho();
		disable_extended_io();
		if (ISSET(PRESERVE)) {
			enable_flow_control();
		}

		disable_signals();
#ifdef USE_SLANG
		if (!ISSET(PRESERVE)) {
			disable_flow_control();
		}

		tcgetattr(0, &newterm);
		newterm_set = true;
	} else {
		tcsetattr(0, TCSANOW, &newterm);
	}
#endif
}

/* Read in a character, interpret it as a shortcut or toggle if
 * necessary, and return it.
 * Set s_or_t to true if the character is a shortcut or toggle
 * key, set ran_func to true if we ran a function associated with a
 * shortcut key, and set finished to true if we're done after running
 * or trying to run a function associated with a shortcut key.  If
 * allow_funcs is false, don't actually run any functions associated
 * with shortcut keys. */
void do_input(void)
{
	Key input = get_kbinput(edit);
	bool preserve = false;
	/* Preserve the contents of the cutbuffer? */

	/* Check for a shortcut in the main list. */
	const sc *s = get_shortcut(input);

	/* If we got a shortcut from the main list, or a "universal"
	 * edit window shortcut, set have_shortcut to true. */
	bool have_shortcut = (s != NULL);

	/* If we got a non-high-bit control key, a meta key sequence, or a
	 * function key, and it's not a shortcut or toggle, throw it out. */
	if (!have_shortcut && (input.has_control_key() || input.has_meta_key())) {
		statusbar(_("Unknown Command"));
		beep();
		return;
	}

	/* If we got a character, and it isn't a shortcut or toggle, it's a
	 * normal text character.  Display a warning if we're in view mode */
	if (!have_shortcut && ISSET(VIEW_MODE)) {
		print_view_warning();
		return;
	}

	/* If we got a shortcut or toggle, and it's not the shortcut
	 * for verbatim input, turn off prepending of wrapped text. */
	if (have_shortcut && s->scfunc != do_verbatim_input) {
		wrap_reset();
	}

	if (!have_shortcut) {
		do_output(input, false);
	} else {
		/* If the function associated with this shortcut is
		* cutting or copying text, indicate this. */
		if (s->scfunc == do_cut_text_void || s->scfunc == do_copy_text || s->scfunc == do_cut_till_eof) {
			preserve = true;
		}

		if (s->scfunc != 0) {
			const subnfunc *f = sctofunc((sc *) s);
			if (ISSET(VIEW_MODE) && f && !f->viewok) {
				print_view_warning();
			} else {
				if (s->scfunc == do_toggle_void) {
					do_toggle(s->toggle);
					if (s->toggle != CUT_TO_END) {
						preserve = true;
					}
				} else {
					s->scfunc();
					if (f && !f->viewok && openfile->syntax != NULL && openfile->syntax->nmultis > 0) {
						reset_multis(openfile->current, false);
					}
					if (edit_refresh_needed) {
						DEBUG_LOG("running edit_refresh() as edit_refresh_needed is true");
						edit_refresh();
						edit_refresh_needed = false;
					}

				}
			}
		}
	}

	/* If we aren't cutting or copying text, blow away the text in the cutbuffer. */
	if (!preserve) {
		cutbuffer_reset();
	}
}

void alloc_multidata_if_needed(filestruct *fileptr)
{
	DEBUG_LOG("alloc_multidata_if_needed");
	if (fileptr->multidata.empty()) {
		DEBUG_LOG("Resizing multidata to size " << openfile->syntax->nmultis);
		fileptr->multidata.resize(openfile->syntax->nmultis);
	}
}

/* Precalculate the multi-line start and end regex info so we can speed up
   rendering (with any hope at all...) */
void precalc_multicolorinfo(void)
{
	DEBUG_LOG("entering precalc_multicolorinfo()");
	if (!openfile->colorstrings.empty() && !ISSET(NO_COLOR_SYNTAX)) {
		regmatch_t startmatch, endmatch;
		filestruct *fileptr, *endptr;
		time_t last_check = time(NULL), cur_check = 0;

		/* Let us get keypresses to see if the user is trying to
		   start editing.  We may want to throw up a statusbar
		   message before starting this later if it takes
		   too long to do this routine.  For now silently
		   abort if they hit a key */
		for (auto tmpcolor : openfile->colorstrings) {

			/* If it's not a multi-line regex, amscray */
			if (tmpcolor->end == NULL) {
				continue;
			}
			DEBUG_LOG("working on color id " << tmpcolor->id);

			for (fileptr = openfile->fileage; fileptr != NULL; fileptr = fileptr->next) {
				int startx = 0;
				int nostart = 0;

				DEBUG_LOG("working on lineno " << fileptr->lineno);

				alloc_multidata_if_needed(fileptr);

				if ((cur_check = time(NULL)) - last_check > 1) {
					last_check = cur_check;
					if (keyboard->has_input()) {
						return;
					}
				}

				while ((nostart = regexec(tmpcolor->start, &fileptr->data[startx], 1, &startmatch, (startx == 0) ? 0 : REG_NOTBOL)) == 0) {
					/* Look for end and start marking how many lines are encompassed
					   whcih should speed up rendering later */
					startx += startmatch.rm_eo;
					DEBUG_LOG("match found at pos " << startx);

					/* Look on this line first for end */
					if (regexec(tmpcolor->end, &fileptr->data[startx], 1, &endmatch, (startx == 0) ? 0 : REG_NOTBOL) == 0) {
						startx += endmatch.rm_eo;
						fileptr->multidata[tmpcolor->id] |= CSTARTENDHERE;
						DEBUG_LOG("end found on this line");
						continue;
					}

					/* Nice, we didn't find the end regex on this line.  Let's start looking for it */
					for (endptr = fileptr->next; endptr != NULL; endptr = endptr->next) {

						DEBUG_LOG("advancing to line " << endptr->lineno << " to find end...");
						/* Check for keyboard input again */
						if ((cur_check = time(NULL)) - last_check > 1) {
							last_check = cur_check;
							if (keyboard->has_input()) {
								return;
							}
						}
						if (regexec(tmpcolor->end, endptr->data, 1, &endmatch, 0) == 0) {
							break;
						}
					}

					if (endptr == NULL) {
						DEBUG_LOG("no end found, breaking out");
						break;
					}


					DEBUG_LOG("end found");

					/* We found it, we found it, la la la la la.  Mark all the
					lines in between and the ends properly */
					fileptr->multidata[tmpcolor->id] |= CENDAFTER;
					DEBUG_LOG("marking line " << fileptr->lineno << " as CENDAFTER");
					for (fileptr = fileptr->next; fileptr != endptr; fileptr = fileptr->next) {
						alloc_multidata_if_needed(fileptr);
						fileptr->multidata[tmpcolor->id] = CWHOLELINE;
						DEBUG_LOG("marking intermediary line " << fileptr->lineno << " as CWHOLELINE");
					}
					alloc_multidata_if_needed(endptr);
					DEBUG_LOG("marking line " << fileptr->lineno << " as CBEGINBEFORE");
					fileptr->multidata[tmpcolor->id] = CBEGINBEFORE;
					/* We should be able to skip all the way to the line of the match.
					This may introduce more bugs but it's the Right Thing to do */
					fileptr = endptr;
					startx = endmatch.rm_eo;
					DEBUG_LOG("jumping to line " << endptr->lineno << " pos " << startx << " to continue");
				}
				if (nostart && startx == 0) {
					DEBUG_LOG("no start found on line " << fileptr->lineno << ", continuing");
					fileptr->multidata[tmpcolor->id] = CNONE;
					continue;
				}
			}
		}
	}
}

/* The user typed output_len multibyte characters.  Add them to the edit
 * buffer, filtering out all ASCII control characters if allow_cntrls is
 * true. */
void do_output(const std::string& output, bool allow_cntrls)
{
	char *str = mallocstrcpy(NULL, output.c_str());
	do_output(str, output.length(), allow_cntrls);
	free(str);
}

void do_output(char *output, size_t output_len, bool allow_cntrls)
{
	size_t current_len, orig_lenpt = 0, i = 0;
	char *char_buf = charalloc(mb_cur_max());
	int char_buf_len;

	assert(openfile->current != NULL && openfile->current->data != NULL);

	current_len = strlen(openfile->current->data);
	if (ISSET(SOFTWRAP)) {
		orig_lenpt = strlenpt(openfile->current->data);
	}

	while (i < output_len) {
		/* If allow_cntrls is true, convert nulls and newlines
		 * properly. */
		if (allow_cntrls) {
			/* Null to newline, if needed. */
			if (output[i] == '\0') {
				output[i] = '\n';
			}
			/* Newline to Enter, if needed. */
			else if (output[i] == '\n') {
				do_enter(false);
				i++;
				continue;
			}
		}

		/* Interpret the next multibyte character. */
		char_buf_len = parse_mbchar(output + i, char_buf, NULL);

		i += char_buf_len;

		/* If allow_cntrls is false, filter out an ASCII control
		 * character. */
		if (!allow_cntrls && is_ascii_cntrl_char(*(output + i - char_buf_len))) {
			continue;
		}

		/* If the NO_NEWLINES flag isn't set, when a character is
		 * added to the magicline, it means we need a new magicline. */
		if (!ISSET(NO_NEWLINES) && openfile->filebot == openfile->current) {
			new_magicline();
		}

		/* More dangerousness fun =) */
		openfile->current->data = charealloc(openfile->current->data, current_len + (char_buf_len * 2));

		assert(openfile->current_x <= current_len);

		charmove(openfile->current->data + openfile->current_x +
		         char_buf_len, openfile->current->data +
		         openfile->current_x, current_len - openfile->current_x +
		         char_buf_len);
		strncpy(openfile->current->data + openfile->current_x, char_buf, char_buf_len);
		current_len += char_buf_len;
		openfile->totsize++;
		set_modified();

		add_undo(ADD);

		/* Note that current_x has not yet been incremented. */
		if (openfile->mark_set && openfile->current == openfile->mark_begin && openfile->current_x < openfile->mark_begin_x) {
			openfile->mark_begin_x += char_buf_len;
		}

		openfile->current_x += char_buf_len;

		update_undo(ADD);

		/* If we're wrapping text, we need to call edit_refresh(). */
		if (!ISSET(NO_WRAP))
			if (do_wrap(openfile->current)) {
				edit_refresh_needed = true;
			}

		/* If color syntaxes are available and turned on, we need to
		 * call edit_refresh(). */
		if (!openfile->colorstrings.empty() && !ISSET(NO_COLOR_SYNTAX)) {
			edit_refresh_needed = true;
		}
	}

	/* Well we might also need a full refresh if we've changed the
	   line length to be a new multiple of COLS */
	if (ISSET(SOFTWRAP) && edit_refresh_needed == false) {
		if (strlenpt(openfile->current->data) / COLS != orig_lenpt / COLS) {
			edit_refresh_needed = true;
		}
	}

	free(char_buf);

	openfile->placewewant = xplustabs();


	reset_multis(openfile->current, false);
	if (edit_refresh_needed == true) {
		edit_refresh();
		edit_refresh_needed = false;
	} else {
		update_line(openfile->current, openfile->current_x);
	}
}

int main(int argc, char **argv)
{
	int optchr;
	ssize_t startline = 0, startcol = 0;
	/* Target line and column when specified on the command line. */

	bool fill_used = false;
	/* Was the fill option used? */
	bool old_multibuffer;
	/* The old value of the multibuffer option, restored after we
	 * load all files on the command line. */
#ifdef HAVE_GETOPT_LONG
	const struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"boldtext", 0, NULL, 'D'},
		{"multibuffer", 0, NULL, 'F'},
		{"ignorercfiles", 0, NULL, 'I'},
		{"rebindkeypad", 0, NULL, 'K'},
		{"nonewlines", 0, NULL, 'L'},
		{"morespace", 0, NULL, 'O'},
		{"tabsize", 1, NULL, 'T'},
		{"version", 0, NULL, 'V'},
		{"syntax", 1, NULL, 'Y'},
		{"const", 0, NULL, 'c'},
		{"nofollow", 0, NULL, 'l'},
		{"preserve", 0, NULL, 'p'},
		{"quiet", 0, NULL, 'q'},
		{"fill", 1, NULL, 'r'},
		{"speller", 1, NULL, 's'},
		{"tempfile", 0, NULL, 't'},
		{"view", 0, NULL, 'v'},
		{"nowrap", 0, NULL, 'w'},
		{"nohelp", 0, NULL, 'x'},
		{"suspend", 0, NULL, 'z'},
		{"smarthome", 0, NULL, 'A'},
		{"backup", 0, NULL, 'B'},
		{"backupdir", 1, NULL, 'C'},
		{"tabstospaces", 0, NULL, 'E'},
		{"locking", 0, NULL, 'G'},
		{"historylog", 0, NULL, 'H'},
		{"noconvert", 0, NULL, 'N'},
		{"poslog", 0, NULL, 'P'},
		{"smooth", 0, NULL, 'S'},
		{"quickblank", 0, NULL, 'U'},
		{"wordbounds", 0, NULL, 'W'},
		{"autoindent", 0, NULL, 'i'},
		{"cut", 0, NULL, 'k'},
		{"softwrap", 0, NULL, '$'},
		{NULL, 0, NULL, 0}
	};
#endif

	{
		/* If the locale set exists and uses UTF-8, we should use
		 * UTF-8. */
		char *locale = setlocale(LC_ALL, "");

		if (locale != NULL && (strcmp(nl_langinfo(CODESET), "UTF-8") == 0)) {
#ifdef USE_SLANG
			SLutf8_enable(1);
#endif
			utf8_init();
		}
	}

#ifdef ENABLE_NLS
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif

	while ((optchr =
#ifdef HAVE_GETOPT_LONG
	            getopt_long(argc, argv, "ABC:DEFGHIKLNOPQ:RST:UVWY:chiklmo:pqr:s:tvwxz$", long_options, NULL)
#else
	            getopt(argc, argv,
	                   "ABC:DEFGHIKLNOPQ:RST:UVWY:chiklmo:pqr:s:tvwxz$")
#endif
	       ) != -1) {
		switch (optchr) {
		case 'A':
			SET(SMART_HOME);
			break;
		case 'B':
			SET(BACKUP_FILE);
			break;
		case 'C':
			backup_dir = optarg;
			break;
		case 'D':
			SET(BOLD_TEXT);
			break;
		case 'E':
			SET(TABS_TO_SPACES);
			break;
		case 'F':
			SET(MULTIBUFFER);
			break;
		case 'G':
			SET(LOCKING);
			break;
		case 'H':
			SET(HISTORYLOG);
			break;
		case 'I':
			no_rcfiles = true;
			break;
		case 'K':
			SET(REBIND_KEYPAD);
			break;
		case 'L':
			SET(NO_NEWLINES);
			break;
		case 'N':
			SET(NO_CONVERT);
			break;
		case 'O':
			SET(MORE_SPACE);
			break;
		case 'P':
			SET(POS_HISTORY);
			break;
		case 'S':
			SET(SMOOTH_SCROLL);
			break;
		case 'T':
			if (!parse_num(optarg, &tabsize) || tabsize <= 0) {
				fprintf(stderr, _("Requested tab size \"%s\" is invalid"), optarg);
				fprintf(stderr, "\n");
				exit(1);
			}
			break;
		case 'U':
			SET(QUICK_BLANK);
			break;
		case 'V':
			version();
			exit(0);
		case 'W':
			SET(WORD_BOUNDS);
			break;
		case 'Y':
			syntaxstr = std::string(optarg);
			break;
		case 'c':
			SET(CONST_UPDATE);
			break;
		case 'h':
			usage();
			exit(0);
		case 'i':
			SET(AUTOINDENT);
			break;
		case 'k':
			SET(CUT_TO_END);
			break;
		case 'l':
			SET(NOFOLLOW_SYMLINKS);
			break;
		case 'p':
			SET(PRESERVE);
			break;
		case 'q':
			SET(QUIET);
			break;
		case 'r':
			if (!parse_num(optarg, &wrap_at)) {
				fprintf(stderr, _("Requested fill size \"%s\" is invalid"), optarg);
				fprintf(stderr, "\n");
				exit(1);
			}
			fill_used = true;
			break;
		case 's':
			alt_speller = mallocstrcpy(alt_speller, optarg);
			break;
		case 't':
			SET(TEMP_FILE);
			break;
		case 'v':
			SET(VIEW_MODE);
			break;
		case 'w':
			SET(NO_WRAP);

			/* If both --fill and --nowrap are given on the command line,
			   the last option wins, */
			fill_used = false;

			break;
		case 'x':
			SET(NO_HELP);
			break;
		case 'z':
			SET(SUSPEND);
			break;
		case '$':
			SET(SOFTWRAP);
			break;
		default:
			printf(_("Type '%s -h' for a list of available options.\n"), argv[0]);
			exit(1);
		}
	}

	/* Set up the shortcut lists.
	   Need to do this before the rcfile */
	shortcut_init();

	/* We've read through the command line options.  Now back up the flags
	 * and values that are set, and read the rcfile(s).  If the values
	 * haven't changed afterward, restore the backed-up values. */
	if (!no_rcfiles) {
		ssize_t wrap_at_cpy = wrap_at;
		char *alt_speller_cpy = alt_speller;
		ssize_t tabsize_cpy = tabsize;
		unsigned flags_cpy[sizeof(flags) / sizeof(flags[0])];
		size_t i;

		memcpy(flags_cpy, flags, sizeof(flags_cpy));

		alt_speller = NULL;

		do_rcfile();

		DEBUG_LOG("After rebinding keys...");
#ifdef DEBUG
		print_sclist();
#endif

		if (fill_used) {
			wrap_at = wrap_at_cpy;
		}
		if (alt_speller_cpy != NULL) {
			free(alt_speller);
			alt_speller = alt_speller_cpy;
		}
		if (tabsize_cpy != -1) {
			tabsize = tabsize_cpy;
		}

		for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
			flags[i] |= flags_cpy[i];
		}
	}
#ifdef DISABLE_ROOTWRAPPING
	/* If we don't have any rcfiles, --disable-wrapping-as-root is used,
	 * and we're root, turn wrapping off. */
	else if (geteuid() == PINOT_ROOT_UID) {
		SET(NO_WRAP);
	}
#endif

	/* Overwrite an rcfile "set nowrap" or --disable-wrapping-as-root
	   if a --fill option was given on the command line. */
	if (fill_used) {
		UNSET(NO_WRAP);
	}

	/* If we're using bold text instead of reverse video text, set it up
	 * now. */
	if (ISSET(BOLD_TEXT)) {
		highlight_attribute = A_BOLD;
	}

	/* Set up the search/replace history. */
	if (!no_rcfiles) {
		if (ISSET(HISTORYLOG) || ISSET(POS_HISTORY)) {
			if (check_dotpinot() == 0) {
				UNSET(HISTORYLOG);
				UNSET(POS_HISTORY);
			}
		}
		if (ISSET(HISTORYLOG)) {
			load_history();
		}
		if (ISSET(POS_HISTORY)) {
			load_poshistory();
		}
	}

	/* Set up the backup  directory. This entails making sure it exists
	 * and is a directory, so that backup files will be saved there. */
	init_backup_dir();

	/* If we don't have an alternative spell checker after reading the
	 * command line and/or rcfile(s), check $SPELL for one, as Pico does */
	if (alt_speller == NULL) {
		char *spellenv = getenv("SPELL");
		if (spellenv != NULL) {
			alt_speller = mallocstrcpy(NULL, spellenv);
		}
	}

	/* If matchbrackets wasn't specified, set its default value. */
	if (matchbrackets == NULL) {
		matchbrackets = mallocstrcpy(NULL, "(<[{)>]}");
	}

	/* If whitespace wasn't specified, set its default value. */
	if (whitespace == "") {
		if (using_utf8()) {
			whitespace = "»·";
			whitespace_len[0] = 2;
			whitespace_len[1] = 2;
		} else {
			whitespace = ">.";
			whitespace_len[0] = 1;
			whitespace_len[1] = 1;
		}
	}

	/* If tabsize wasn't specified, set its default value. */
	if (tabsize == -1) {
		tabsize = WIDTH_OF_TAB;
	}

	/* Back up the old terminal settings so that they can be restored. */
	tcgetattr(0, &oldterm);

	/* Initialize curses mode.  If this fails, get out. */
	if (initscr() == NULL) {
		exit(1);
	}

	/* Set up the terminal state. */
	terminal_init();

	/* Turn the cursor on for sure. */
	curs_set(1);

	DEBUG_LOG("Main: set up windows");

	/* Initialize all the windows based on the current screen dimensions. */
	window_init();

	/* Initialize keyboard input */
	keyboard_init();

	/* Set up the signal handlers. */
	signal_init();

	set_colorpairs();

	DEBUG_LOG("Main: open file");

	/* If there's a +LINE or +LINE,COLUMN flag here, it is the first
	 * non-option argument, and it is followed by at least one other
	 * argument, the filename it applies to. */
	if (0 < optind && optind < argc - 1 && argv[optind][0] == '+') {
		parse_line_column(&argv[optind][1], &startline, &startcol);
		optind++;
	}

	if (optind < argc && !strcmp(argv[optind], "-")) {
		stdin_pager();
		set_modified();
		optind++;
	}

	old_multibuffer = ISSET(MULTIBUFFER);
	SET(MULTIBUFFER);

	/* Read all the files after the first one on the command line into
	 * new buffers. */
	{
		int i = optind + 1;
		ssize_t iline = 0, icol = 0;

		for (; i < argc; i++) {
			/* If there's a +LINE or +LINE,COLUMN flag here, it is
			 * followed by at least one other argument, the filename it
			 * applies to. */
			if (i < argc - 1 && argv[i][0] == '+' && iline == 1 && icol == 1) {
				parse_line_column(&argv[i][1], &iline, &icol);
			} else {
				open_buffer(argv[i], false);

				if (iline > 0 || icol > 0) {
					do_gotolinecolumn(iline, icol, false, false, false, false);
					iline = 1;
					icol = 1;
				} else {
					/* See if we have a POS history to use if we haven't overridden it */
					ssize_t savedposline, savedposcol;
					if (check_poshistory(argv[i], &savedposline, &savedposcol)) {
						do_gotolinecolumn(savedposline, savedposcol, false, false, false, false);
					}
				}
			}
		}
	}

	/* Read the first file on the command line into either the current
	 * buffer or a new buffer, depending on whether multibuffer mode is
	 * enabled. */
	if (optind < argc) {
		open_buffer(argv[optind], false);
	}

	/* We didn't open any files if all the command line arguments were
	 * invalid files like directories or if there were no command line
	 * arguments given.  In this case, we have to load a blank buffer.
	 * Also, we unset view mode to allow editing. */
	if (openfiles.size() == 0) {
		open_buffer("", false);
		UNSET(VIEW_MODE);
	}

	if (!old_multibuffer) {
		UNSET(MULTIBUFFER);
	}

	DEBUG_LOG("Main: top and bottom win");

	if (openfile->syntax && openfile->syntax->nmultis > 0) {
		precalc_multicolorinfo();
	}

	if (startline > 0 || startcol > 0) {
		do_gotolinecolumn(startline, startcol, false, false, false, false);
	} else {
		/* See if we have a POS history to use if we haven't overridden it */
		ssize_t savedposline, savedposcol;
		if (argv[optind] && check_poshistory(argv[optind], &savedposline, &savedposcol)) {
			do_gotolinecolumn(savedposline, savedposcol, false, false, false, false);
		}
	}

	display_main_list();

	display_buffer();

	while (true) {
		/* Make sure the cursor is in the edit window. */
		reset_cursor();
		wnoutrefresh(edit);

		if (!jump_buf_set) {
			/* If we haven't already, we're going to set jump_buf so
			 * that we return here after a SIGWINCH.  Indicate this. */
			jump_buf_set = true;

			/* Return here after a SIGWINCH. */
			sigsetjmp(jump_buf, 1);
		}

		/* Just in case we were at the statusbar prompt, make sure the
		 * statusbar cursor position is reset. */
		do_prompt_abort();

		/* If constant cursor position display is on, display the current cursor position on the statusbar. */
		if (ISSET(CONST_UPDATE)) {
			do_cursorpos(true);
		}

		currmenu = MMAIN;

		/* Read in and interpret characters. */
		do_input();
	}

	/* We should never get here. */
	assert(false);
}
