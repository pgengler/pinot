/**************************************************************************
 *   text.c                                                               *
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
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

static pid_t pid = -1;
/* The PID of the forked process in execute_command(), for use
 * with the cancel_command() signal handler. */
static bool prepend_wrap = FALSE;
/* Should we prepend wrapped text to the next line? */

/* Toggle the mark. */
void do_mark(void)
{
	openfile->mark_set = !openfile->mark_set;
	if (openfile->mark_set) {
		statusbar(_("Mark Set"));
		openfile->mark_begin = openfile->current;
		openfile->mark_begin_x = openfile->current_x;
	} else {
		statusbar(_("Mark Unset"));
		openfile->mark_begin = NULL;
		openfile->mark_begin_x = 0;
		edit_refresh();
	}
}

/* Delete the character under the cursor. */
void do_delete(void)
{
	size_t orig_lenpt = 0;

	update_undo(DEL);

	assert(openfile->current != NULL && openfile->current->data != NULL && openfile->current_x <= strlen(openfile->current->data));

	openfile->placewewant = xplustabs();

	if (openfile->current->data[openfile->current_x] != '\0') {
		int char_buf_len = parse_mbchar(openfile->current->data + openfile->current_x, NULL, NULL);
		size_t line_len = strlen(openfile->current->data + openfile->current_x);

		assert(openfile->current_x < strlen(openfile->current->data));

		if (ISSET(SOFTWRAP)) {
			orig_lenpt = strlenpt(openfile->current->data);
		}

		/* Let's get dangerous. */
		charmove(&openfile->current->data[openfile->current_x], &openfile->current->data[openfile->current_x + char_buf_len], line_len - char_buf_len + 1);

		null_at(&openfile->current->data, openfile->current_x + line_len - char_buf_len);
		if (openfile->mark_set && openfile->mark_begin == openfile->current && openfile->current_x < openfile->mark_begin_x) {
			openfile->mark_begin_x -= char_buf_len;
		}
		openfile->totsize--;
	} else if (openfile->current != openfile->filebot) {
		filestruct *foo = openfile->current->next;

		assert(openfile->current_x == strlen(openfile->current->data));

		/* If we're deleting at the end of a line, we need to call edit_refresh(). */
		if (openfile->current->data[openfile->current_x] == '\0') {
			edit_refresh_needed = TRUE;
		}

		openfile->current->data = charealloc(openfile->current->data, openfile->current_x + strlen(foo->data) + 1);
		strcpy(openfile->current->data + openfile->current_x, foo->data);
		if (openfile->mark_set && openfile->mark_begin == openfile->current->next) {
			openfile->mark_begin = openfile->current;
			openfile->mark_begin_x += openfile->current_x;
		}
		if (openfile->filebot == foo) {
			openfile->filebot = openfile->current;
		}

		unlink_node(foo);
		delete_node(foo);
		renumber(openfile->current);
		openfile->totsize--;

		/* If the NO_NEWLINES flag isn't set, and text has been added to
		 * the magicline as a result of deleting at the end of the line
		 * before filebot, add a new magicline. */
		if (!ISSET(NO_NEWLINES) && openfile->current == openfile->filebot && openfile->current->data[0] != '\0') {
			new_magicline();
		}
	} else {
		return;
	}

	if (ISSET(SOFTWRAP) && edit_refresh_needed == FALSE)
		if (strlenpt(openfile->current->data) / COLS != orig_lenpt / COLS) {
			edit_refresh_needed  = TRUE;
		}

	set_modified();

	if (edit_refresh_needed  == FALSE) {
		update_line(openfile->current, openfile->current_x);
	}
}

/* Backspace over one character.  That is, move the cursor left one
 * character, and then delete the character under the cursor. */
void do_backspace(void)
{
	if (openfile->current != openfile->fileage || openfile->current_x > 0) {
		do_left();
		do_delete();
	}
}

/* Insert a tab.  If the TABS_TO_SPACES flag is set, insert the number
 * of spaces that a tab would normally take up. */
void do_tab(void)
{
	if (ISSET(TABS_TO_SPACES)) {
		char *output;
		size_t output_len = 0, new_pww = xplustabs();

		do {
			new_pww++;
			output_len++;
		} while (new_pww % tabsize != 0);

		output = charalloc(output_len + 1);

		charset(output, ' ', output_len);
		output[output_len] = '\0';

		do_output(output, output_len, TRUE);

		free(output);
	} else {
		do_output((char *) "\t", 1, TRUE);
	}
}

/* Indent or unindent the current line (or, if the mark is on, all lines
 * covered by the mark) len columns, depending on whether len is
 * positive or negative.  If the TABS_TO_SPACES flag is set, indent or
 * unindent by len spaces.  Otherwise, indent or unindent by (len /
 * tabsize) tabs and (len % tabsize) spaces. */
void do_indent(ssize_t cols)
{
	bool indent_changed = FALSE;
	/* Whether any indenting or unindenting was done. */
	bool unindent = FALSE;
	/* Whether we're unindenting text. */
	char *line_indent = NULL;
	/* The text added to each line in order to indent it. */
	size_t line_indent_len = 0;
	/* The length of the text added to each line in order to indent it. */
	filestruct *top, *bot, *f;
	size_t top_x, bot_x;

	assert(openfile->current != NULL && openfile->current->data != NULL);

	/* If cols is zero, get out. */
	if (cols == 0) {
		return;
	}

	/* If cols is negative, make it positive and set unindent to TRUE. */
	if (cols < 0) {
		cols = -cols;
		unindent = TRUE;
		/* Otherwise, we're indenting, in which case the file will always be
		 * modified, so set indent_changed to TRUE. */
	} else {
		indent_changed = TRUE;
	}

	if (openfile->mark_set) {
		/* If the mark is on, use all lines covered by the mark. */
		mark_order((const filestruct **)&top, &top_x, (const filestruct **)&bot, &bot_x, NULL);
	} else {
		/* Otherwise, use the current line. */
		top = openfile->current;
		bot = top;
	}

	if (!unindent) {
		/* Set up the text we'll be using as indentation. */
		line_indent = charalloc(cols + 1);

		if (ISSET(TABS_TO_SPACES)) {
			/* Set the indentation to cols spaces. */
			charset(line_indent, ' ', cols);
			line_indent_len = cols;
		} else {
			/* Set the indentation to (cols / tabsize) tabs and (cols % tabsize) spaces. */
			size_t num_tabs = cols / tabsize;
			size_t num_spaces = cols % tabsize;

			charset(line_indent, '\t', num_tabs);
			charset(line_indent + num_tabs, ' ', num_spaces);

			line_indent_len = num_tabs + num_spaces;
		}

		line_indent[line_indent_len] = '\0';
	}

	/* Go through each line of the text. */
	for (f = top; f != bot->next; f = f->next) {
		size_t line_len = strlen(f->data);
		size_t indent_len = indent_length(f->data);

		if (line_len == 0) {
			continue;
		}

		if (!unindent) {
			/* If we're indenting, add the characters in line_indent to
			 * the beginning of the non-whitespace text of this line. */
			f->data = charealloc(f->data, line_len + line_indent_len + 1);
			charmove(&f->data[indent_len + line_indent_len], &f->data[indent_len], line_len - indent_len + 1);
			strncpy(f->data + indent_len, line_indent, line_indent_len);
			openfile->totsize += line_indent_len;

			/* Keep track of the change in the current line. */
			if (openfile->mark_set && f == openfile->mark_begin && openfile->mark_begin_x >= indent_len) {
				openfile->mark_begin_x += line_indent_len;
			}

			if (f == openfile->current && openfile->current_x >= indent_len) {
				openfile->current_x += line_indent_len;
			}

			/* If the NO_NEWLINES flag isn't set, and this is the
			 * magicline, add a new magicline. */
			if (!ISSET(NO_NEWLINES) && f == openfile->filebot) {
				new_magicline();
			}
		} else {
			size_t indent_col = strnlenpt(f->data, indent_len);
			/* The length in columns of the indentation on this line. */

			if (cols <= indent_col) {
				size_t indent_new = actual_x(f->data, indent_col - cols);
				/* The length of the indentation remaining on
				 * this line after we unindent. */
				size_t indent_shift = indent_len - indent_new;
				/* The change in the indentation on this line
				 * after we unindent. */

				/* If we're unindenting, and there's at least cols
				 * columns' worth of indentation at the beginning of the
				 * non-whitespace text of this line, remove it. */
				charmove(&f->data[indent_new], &f->data[indent_len], line_len - indent_shift - indent_new + 1);
				null_at(&f->data, line_len - indent_shift + 1);
				openfile->totsize -= indent_shift;

				/* Keep track of the change in the current line. */
				if (openfile->mark_set && f == openfile->mark_begin && openfile->mark_begin_x > indent_new) {
					if (openfile->mark_begin_x <= indent_len) {
						openfile->mark_begin_x = indent_new;
					} else {
						openfile->mark_begin_x -= indent_shift;
					}
				}

				if (f == openfile->current && openfile->current_x > indent_new) {
					if (openfile->current_x <= indent_len) {
						openfile->current_x = indent_new;
					} else {
						openfile->current_x -= indent_shift;
					}
				}

				/* We've unindented, so set indent_changed to TRUE. */
				if (!indent_changed) {
					indent_changed = TRUE;
				}
			}
		}
	}

	if (!unindent) {
		/* Clean up. */
		free(line_indent);
	}

	if (indent_changed) {
		/* Mark the file as modified. */
		set_modified();

		/* Update the screen. */
		edit_refresh_needed = TRUE;
	}
}

/* Indent the current line, or all lines covered by the mark if the mark
 * is on, tabsize columns. */
void do_indent_void(void)
{
	do_indent(tabsize);
}

/* Unindent the current line, or all lines covered by the mark if the
 * mark is on, tabsize columns. */
void do_unindent(void)
{
	do_indent(-tabsize);
}

/* undo a cut, or re-do an uncut */
void undo_cut(undo *u)
{
	/* If we cut the magicline may was well not crash :/ */
	if (!u->cutbuffer) {
		return;
	}

	cutbuffer = copy_filestruct(u->cutbuffer);

	/* Compute cutbottom for the uncut using out copy */
	for (cutbottom = cutbuffer; cutbottom->next != NULL; cutbottom = cutbottom->next) {
		;
	}

	/* Get to where we need to uncut from */
	if (u->mark_set && u->mark_begin_lineno < u->lineno) {
		do_gotolinecolumn(u->mark_begin_lineno, u->mark_begin_x+1, FALSE, FALSE, FALSE, FALSE);
	} else {
		do_gotolinecolumn(u->lineno, u->begin+1, FALSE, FALSE, FALSE, FALSE);
	}

	copy_from_filestruct(cutbuffer, cutbottom);
	free_filestruct(cutbuffer);
	cutbuffer = NULL;

}

/* Re-do a cut, or undo an uncut */
void redo_cut(undo *u)
{
	int i;
	filestruct *t, *c;

	/* If we cut the magicline may was well not crash :/ */
	if (!u->cutbuffer) {
		return;
	}

	do_gotolinecolumn(u->lineno, u->begin+1, FALSE, FALSE, FALSE, FALSE);
	openfile->mark_set = u->mark_set;
	if (cutbuffer) {
		free(cutbuffer);
	}
	cutbuffer = NULL;

	/* Move ahead the same # lines we had if a marked cut */
	if (u->mark_set) {
		for (i = 1, t = openfile->fileage; i != u->mark_begin_lineno; i++) {
			t = t->next;
		}
		openfile->mark_begin = t;
	} else if (!u->to_end) {
		/* Here we have a regular old potentially multi-line ^K cut.  We'll
		   need to trick pinot into thinking it's a marked cut to cut more
		   than one line again */
		for (c = u->cutbuffer, t = openfile->current; c->next != NULL && t->next != NULL; ) {

			DEBUG_LOG("Advancing, lineno  = " << t->lineno << ", data = \"" << t->data << '"');
			c = c->next;
			t = t->next;
		}
		openfile->mark_begin = t;
		openfile->mark_begin_x = 0;
		openfile->mark_set = TRUE;
	}

	openfile->mark_begin_x = u->mark_begin_x;
	do_cut_text(FALSE, u->to_end, TRUE);
	openfile->mark_set = FALSE;
	openfile->mark_begin = NULL;
	openfile->mark_begin_x = 0;
	edit_refresh_needed = TRUE;
}

/* Undo the last thing(s) we did */
void do_undo(void)
{
	undo *u = openfile->current_undo;
	filestruct *f = openfile->current, *t;
	int len = 0;
	char *undidmsg, *data;
	filestruct *oldcutbuffer = cutbuffer, *oldcutbottom = cutbottom;

	if (!u) {
		statusbar(_("Nothing in undo buffer!"));
		return;
	}

	if (u->lineno <= f->lineno) {
		for (; f->prev != NULL && f->lineno != u->lineno; f = f->prev) {
			;
		}
	} else {
		for (; f->next != NULL && f->lineno != u->lineno; f = f->next) {
			;
		}
	}
	if (f->lineno != u->lineno) {
		statusbar(_("Internal error: can't match line %d.  Please save your work."), u->lineno);
		return;
	}
	DEBUG_LOG("data we're about to undo = \"" << f->data << '"');
	DEBUG_LOG("Undo running for type " << u->type);

	openfile->current_x = u->begin;
	switch(u->type) {
	case ADD:
		undidmsg = _("text add");
		len = strlen(f->data) - strlen(u->strdata) + 1;
		data = charalloc(len);
		strncpy(data, f->data, u->begin);
		strcpy(&data[u->begin], &f->data[u->begin + strlen(u->strdata)]);
		free(f->data);
		f->data = data;
		break;
	case DEL:
		undidmsg = _("text delete");
		len = strlen(f->data) + strlen(u->strdata) + 1;
		data = charalloc(len);

		strncpy(data, f->data, u->begin);
		strcpy(&data[u->begin], u->strdata);
		strcpy(&data[u->begin + strlen(u->strdata)], &f->data[u->begin]);
		free(f->data);
		f->data = data;
		if (u->xflags == UNdel_backspace) {
			openfile->current_x += strlen(u->strdata);
		}
		break;
	case SPLIT:
		undidmsg = _("line wrap");
		f->data = (char *) nrealloc(f->data, strlen(f->data) + strlen(u->strdata) + 1);
		strcpy(&f->data[strlen(f->data) - 1], u->strdata);
		if (u->strdata2 != NULL) {
			f->next->data = mallocstrcpy(f->next->data, u->strdata2);
		} else {
			filestruct *foo = openfile->current->next;
			unlink_node(foo);
			delete_node(foo);
		}
		renumber(f);
		break;
	case UNSPLIT:
		undidmsg = _("line join");
		t = make_new_node(f);
		t->data = mallocstrcpy(NULL, u->strdata);
		data = mallocstrncpy(NULL, f->data, u->begin);
		data[u->begin] = '\0';
		free(f->data);
		f->data = data;
		splice_node(f, t, f->next);
		renumber(f);
		break;
	case CUT:
		undidmsg = _("text cut");
		undo_cut(u);
		break;
	case UNCUT:
		undidmsg = _("text uncut");
		redo_cut(u);
		break;
	case ENTER:
		undidmsg = _("line break");
		if (f->next) {
			filestruct *foo = f->next;
			f->data = (char *) nrealloc(f->data, strlen(f->data) + strlen(f->next->data) + 1);
			strcat(f->data,  f->next->data);
			unlink_node(foo);
			delete_node(foo);
		}
		break;
	case INSERT:
		undidmsg = _("text insert");
		cutbuffer = NULL;
		cutbottom = NULL;
		/* When we updated mark_begin_lineno in update_undo, it was effectively how many line
		   were inserted due to being partitioned before read_file was called.  So we
		   add its value here */
		openfile->mark_begin = fsfromline(u->lineno + u->mark_begin_lineno - 1);
		openfile->mark_begin_x = 0;
		openfile->mark_set = TRUE;
		do_gotolinecolumn(u->lineno, u->begin+1, FALSE, FALSE, FALSE, FALSE);
		cut_marked();
		u->cutbuffer = cutbuffer;
		u->cutbottom = cutbottom;
		cutbuffer = oldcutbuffer;
		cutbottom = oldcutbottom;
		openfile->mark_set = FALSE;
		break;
	case REPLACE:
		undidmsg = _("text replace");
		data = u->strdata;
		u->strdata = f->data;
		f->data = data;
		break;

	default:
		undidmsg = _("Internal error: unknown type.  Please save your work.");
		break;

	}
	renumber(f);
	do_gotolinecolumn(u->lineno, u->begin, FALSE, FALSE, FALSE, TRUE);
	statusbar(_("Undid action (%s)"), undidmsg);
	openfile->current_undo = openfile->current_undo->next;
	openfile->last_action = OTHER;
}

void do_redo(void)
{
	undo *u = openfile->undotop;
	filestruct *f = openfile->current;
	int len = 0;
	char *undidmsg, *data;

	for (; u != NULL && u->next != openfile->current_undo; u = u->next) {
		;
	}
	if (!u) {
		statusbar(_("Nothing to re-do!"));
		return;
	}
	if (u->next != openfile->current_undo) {
		statusbar(_("Internal error: Redo setup failed.  Please save your work."));
		return;
	}

	if (u->lineno <= f->lineno)
		for (; f->prev != NULL && f->lineno != u->lineno; f = f->prev) {
			;
		}
	else
		for (; f->next != NULL && f->lineno != u->lineno; f = f->next) {
			;
		}
	if (f->lineno != u->lineno) {
		statusbar(_("Internal error: can't match line %d.  Please save your work."), u->lineno);
		return;
	}
	DEBUG_LOG("data we're about to redo = \"" << f->data << '"');
	DEBUG_LOG("Redo running for type " << u->type);

	switch(u->type) {
	case ADD:
		undidmsg = _("text add");
		len = strlen(f->data) + strlen(u->strdata) + 1;
		data = charalloc(len);
		strncpy(data, f->data, u->begin);
		strcpy(&data[u->begin], u->strdata);
		strcpy(&data[u->begin + strlen(u->strdata)], &f->data[u->begin]);
		free(f->data);
		f->data = data;
		break;
	case DEL:
		undidmsg = _("text delete");
		len = strlen(f->data) + strlen(u->strdata) + 1;
		data = charalloc(len);
		strncpy(data, f->data, u->begin);
		strcpy(&data[u->begin], &f->data[u->begin + strlen(u->strdata)]);
		free(f->data);
		f->data = data;
		break;
	case ENTER:
		undidmsg = _("line break");
		do_gotolinecolumn(u->lineno, u->begin+1, FALSE, FALSE, FALSE, FALSE);
		do_enter(TRUE);
		break;
	case SPLIT:
		undidmsg = _("line wrap");
		if (u->xflags & UNsplit_madenew) {
			prepend_wrap = TRUE;
		}
		do_wrap(f, TRUE);
		renumber(f);
		break;
	case UNSPLIT:
		undidmsg = _("line join");
		len = strlen(f->data) + strlen(u->strdata + 1);
		data = charalloc(len);
		strcpy(data, f->data);
		strcat(data, u->strdata);
		free(f->data);
		f->data = data;
		if (f->next != NULL) {
			filestruct *tmp = f->next;
			unlink_node(tmp);
			delete_node(tmp);
		}
		renumber(f);
		break;
	case CUT:
		undidmsg = _("text cut");
		redo_cut(u);
		break;
	case UNCUT:
		undidmsg = _("text uncut");
		undo_cut(u);
		break;
	case REPLACE:
		undidmsg = _("text replace");
		data = u->strdata;
		u->strdata = f->data;
		f->data = data;
		break;
	case INSERT:
		undidmsg = _("text insert");
		do_gotolinecolumn(u->lineno, u->begin+1, FALSE, FALSE, FALSE, FALSE);
		copy_from_filestruct(u->cutbuffer, u->cutbottom);
		openfile->placewewant = xplustabs();
		break;
	default:
		undidmsg = _("Internal error: unknown type.  Please save your work.");
		break;

	}
	do_gotolinecolumn(u->lineno, u->begin, FALSE, FALSE, FALSE, TRUE);
	statusbar(_("Redid action (%s)"), undidmsg);

	openfile->current_undo = u;
	openfile->last_action = OTHER;

}

/* Someone hits Enter *gasp!* */
void do_enter(bool undoing)
{
	filestruct *newnode = make_new_node(openfile->current);
	size_t extra = 0;

	assert(openfile->current != NULL && openfile->current->data != NULL);

	if (!undoing) {
		add_undo(ENTER);
	}

	/* Do auto-indenting, like the neolithic Turbo Pascal editor. */
	if (ISSET(AUTOINDENT)) {
		/* If we are breaking the line in the indentation, the new
		 * indentation should have only current_x characters, and
		 * current_x should not change. */
		extra = indent_length(openfile->current->data);
		if (extra > openfile->current_x) {
			extra = openfile->current_x;
		}
	}

	newnode->data = charalloc(strlen(openfile->current->data + openfile->current_x) + extra + 1);
	strcpy(&newnode->data[extra], openfile->current->data + openfile->current_x);

	if (ISSET(AUTOINDENT)) {
		strncpy(newnode->data, openfile->current->data, extra);
		openfile->totsize += extra;
	}

	null_at(&openfile->current->data, openfile->current_x);

	if (openfile->mark_set && openfile->current == openfile->mark_begin && openfile->current_x < openfile->mark_begin_x) {
		openfile->mark_begin = newnode;
		openfile->mark_begin_x += extra - openfile->current_x;
	}

	openfile->current_x = extra;

	if (openfile->current == openfile->filebot) {
		openfile->filebot = newnode;
	}
	splice_node(openfile->current, newnode, openfile->current->next);

	renumber(openfile->current);
	openfile->current = newnode;

	openfile->totsize++;
	set_modified();

	openfile->placewewant = xplustabs();

	edit_refresh_needed = TRUE;
}

/* Need this again... */
void do_enter_void(void)
{
	do_enter(FALSE);
}

/* Send a SIGKILL (unconditional kill) to the forked process in
 * execute_command(). */
void cancel_command(int signal)
{
	UNUSED_VAR(signal);
	if (kill(pid, SIGKILL) == -1) {
		nperror("kill");
	}
}

/* Execute command in a shell.  Return TRUE on success. */
bool execute_command(const char *command)
{
	int fd[2];
	FILE *f;
	char *shellenv;
	struct sigaction oldaction, newaction;
	/* Original and temporary handlers for SIGINT. */
	bool sig_failed = FALSE;
	/* Did sigaction() fail without changing the signal handlers? */

	/* Make our pipes. */
	if (pipe(fd) == -1) {
		statusbar(_("Could not create pipe"));
		return FALSE;
	}

	/* Check $SHELL for the shell to use.  If it isn't set, use /bin/sh.
	 * Note that $SHELL should contain only a path, with no arguments. */
	shellenv = getenv("SHELL");
	if (shellenv == NULL) {
		shellenv = (char *) "/bin/sh";
	}

	/* Fork a child. */
	if ((pid = fork()) == 0) {
		close(fd[0]);
		dup2(fd[1], fileno(stdout));
		dup2(fd[1], fileno(stderr));

		/* If execl() returns at all, there was an error. */
		execl(shellenv, tail(shellenv), "-c", command, NULL);
		exit(0);
	}

	/* Continue as parent. */
	close(fd[1]);

	if (pid == -1) {
		close(fd[0]);
		statusbar(_("Could not fork"));
		return FALSE;
	}

	/* Before we start reading the forked command's output, we set
	 * things up so that Ctrl-C will cancel the new process. */

	/* Enable interpretation of the special control keys so that we get
	 * SIGINT when Ctrl-C is pressed. */
	enable_signals();

	if (sigaction(SIGINT, NULL, &newaction) == -1) {
		sig_failed = TRUE;
		nperror("sigaction");
	} else {
		newaction.sa_handler = cancel_command;
		if (sigaction(SIGINT, &newaction, &oldaction) == -1) {
			sig_failed = TRUE;
			nperror("sigaction");
		}
	}

	/* Note that now oldaction is the previous SIGINT signal handler,
	 * to be restored later. */

	f = fdopen(fd[0], "rb");
	if (f == NULL) {
		nperror("fdopen");
	}

	read_file(f, 0, "stdin", TRUE, FALSE);

	if (wait(NULL) == -1) {
		nperror("wait");
	}

	if (!sig_failed && sigaction(SIGINT, &oldaction, NULL) == -1) {
		nperror("sigaction");
	}

	/* Restore the terminal to its previous state.  In the process,
	 * disable interpretation of the special control keys so that we can
	 * use Ctrl-C for other things. */
	terminal_init();

	return TRUE;
}

/* Execute command in a shell without saving its output. Returns -1 if an
 * an error prevented execution, and the command's exit code if it was run. */
int execute_command_silently(const char *command)
{
	int fd[2];
	char *shellenv;
	struct sigaction oldaction, newaction;
	/* Original and temporary handlers for SIGINT. */
	bool sig_failed = FALSE;
	/* Did sigaction() fail without changing the signal handlers? */
	int status;

	/* Make our pipes. */
	if (pipe(fd) == -1) {
		statusbar(_("Could not pipe"));
		return FALSE;
	}

	/* Check $SHELL for the shell to use.  If it isn't set, use /bin/sh.
	 * Note that $SHELL should contain only a path, with no arguments. */
	shellenv = getenv("SHELL");
	if (shellenv == NULL) {
		shellenv = (char *) "/bin/sh";
	}

	/* Fork a child. */
	if ((pid = fork()) == 0) {
		close(fd[0]);
		close(fileno(stdout));
		close(fileno(stderr));

		/* If execl() returns at all, there was an error. */
		execl(shellenv, tail(shellenv), "-c", command, NULL);
		exit(0);
	}

	/* Continue as parent. */
	close(fd[1]);

	if (pid == -1) {
		close(fd[0]);
		statusbar(_("Could not fork"));
		return -1;
	}

	/* Set things up so that Ctrl-C will cancel the new process. */

	/* Enable interpretation of the special control keys so that we get
	 * SIGINT when Ctrl-C is pressed. */
	enable_signals();

	if (sigaction(SIGINT, NULL, &newaction) == -1) {
		sig_failed = TRUE;
		nperror("sigaction");
	} else {
		newaction.sa_handler = cancel_command;
		if (sigaction(SIGINT, &newaction, &oldaction) == -1) {
			sig_failed = TRUE;
			nperror("sigaction");
		}
	}

	/* Note that now oldaction is the previous SIGINT signal handler,
	 * to be restored later. */

	if (wait(&status) == -1) {
		nperror("wait");
	}

	if (!sig_failed && sigaction(SIGINT, &oldaction, NULL) == -1) {
		nperror("sigaction");
	}

	/* Restore the terminal to its previous state.  In the process,
	 * disable interpretation of the special control keys so that we can
	 * use Ctrl-C for other things. */
	terminal_init();

	return WEXITSTATUS(status);
}

/* Add a new undo struct to the top of the current pile */
void add_undo(undo_type current_action)
{
	undo *u;
	char *data;
	openfilestruct *fs = openfile;
	static undo *last_cutu = NULL; /* Last thing we cut to set up the undo for uncut */
	ssize_t wrap_loc;	/* For calculating split beginning */

	if (!ISSET(UNDOABLE)) {
		return;
	}

	/* Ugh, if we were called while cutting not-to-end, non-marked and on the same lineno,
	   we need to  abort here */
	u = fs->current_undo;
	if (current_action == CUT && u && u->type == CUT && !u->mark_set && u->lineno == fs->current->lineno) {
		return;
	}

	/* Blow away the old undo stack if we are starting from the middle */
	while (fs->undotop != NULL && fs->undotop != fs->current_undo) {
		undo *u2 = fs->undotop;
		fs->undotop = fs->undotop->next;
		if (u2->strdata != NULL) {
			free(u2->strdata);
		}
		if (u2->cutbuffer) {
			free_filestruct(u2->cutbuffer);
		}
		free(u2);
	}

	/* Allocate and initialize a new undo type */
	u = (undo *) nmalloc(sizeof(undo));
	u->type = current_action;
	u->lineno = fs->current->lineno;
	u->begin = fs->current_x;
	u->next = fs->undotop;
	fs->undotop = u;
	fs->current_undo = u;
	u->strdata = NULL;
	u->strdata2 = NULL;
	u->cutbuffer = NULL;
	u->cutbottom  = NULL;
	u->mark_set = 0;
	u->mark_begin_lineno = 0;
	u->mark_begin_x = 0;
	u->xflags = 0;
	u->to_end = FALSE;

	switch (u->type) {
		/* We need to start copying data into the undo buffer or we wont be able
		   to restore it later */
	case ADD:
		data = charalloc(2);
		data[0] = fs->current->data[fs->current_x];
		data[1] = '\0';
		u->strdata = data;
		break;
	case DEL:
		if (u->begin != strlen(fs->current->data)) {
			data = mallocstrncpy(NULL, &fs->current->data[u->begin], 2);
			data[1] = '\0';
			u->strdata = data;
			break;
		}
		/* Else purposely fall into unsplit code */
		current_action = u->type = UNSPLIT;
	case UNSPLIT:
		if (fs->current->next) {
			data = mallocstrcpy(NULL, fs->current->next->data);
			u->strdata = data;
		}
		break;
	case SPLIT:
		wrap_loc = break_line(openfile->current->data, fill, FALSE);
		u->strdata = mallocstrcpy(NULL, &openfile->current->data[wrap_loc]);
		/* Don't both saving the next line if we're not prepending as a new line
		   will be created */
		if (prepend_wrap) {
			u->strdata2 = mallocstrcpy(NULL, fs->current->next->data);
		}
		u->begin = wrap_loc;
		break;
	case INSERT:
	case REPLACE:
		data = mallocstrcpy(NULL, fs->current->data);
		u->strdata = data;
		break;
	case CUT:
		u->mark_set = openfile->mark_set;
		if (u->mark_set) {
			u->mark_begin_lineno = openfile->mark_begin->lineno;
			u->mark_begin_x = openfile->mark_begin_x;
		}
		u->to_end = (ISSET(CUT_TO_END)) ? TRUE : FALSE;
		last_cutu = u;
		break;
	case UNCUT:
		if (!last_cutu) {
			statusbar(_("Internal error: can't setup uncut.  Please save your work."));
		} else if (last_cutu->type == CUT) {
			u->cutbuffer = last_cutu->cutbuffer;
			u->cutbottom = last_cutu->cutbottom;
		}
		break;
	case ENTER:
		break;
	case OTHER:
		statusbar(_("Internal error: unknown type.  Please save your work."));
		break;
	}

	DEBUG_LOG("fs->current->data = \"" << fs->current->data << "\", current_x = " << fs->current_x << ", u->begin = " << u->begin << ", type = " << current_action);
	DEBUG_LOG("left add_undo...");
	fs->last_action = current_action;
}

/* Update an undo item, or determine whether a new one
   is really needed and bounce the data to add_undo
   instead.  The latter functionality just feels
   gimmicky and may just be more hassle than
   it's worth, so it should be axed if needed. */
void update_undo(undo_type action)
{
	undo *u;
	char *data;
	int len = 0;
	openfilestruct *fs = openfile;

	if (!ISSET(UNDOABLE)) {
		return;
	}

	DEBUG_LOG("action = " << action << ", fs->last_action = " << fs->last_action << ",  openfile->current->lineno = " << openfile->current->lineno);
	if (fs->current_undo) {
		DEBUG_LOG("fs->current_undo->lineno = " << fs->current_undo->lineno);
	}

	/* Change to an add if we're not using the same undo struct
	   that we should be using */
	if (action != fs->last_action || (action != CUT && action != INSERT && action != SPLIT && openfile->current->lineno != fs->current_undo->lineno)) {
		add_undo(action);
		return;
	}

	assert(fs->undotop != NULL);
	u = fs->undotop;

	switch (u->type) {
	case ADD:
		DEBUG_LOG("fs->current->data = \"" << fs->current->data << "\", current_x = " << fs->current_x << ", u->begin = " << u->begin);
		len = strlen(u->strdata) + 2;
		data = (char *) nrealloc((void *) u->strdata, len * sizeof(char *));
		data[len-2] = fs->current->data[fs->current_x];
		data[len-1] = '\0';
		u->strdata = (char *) data;
		DEBUG_LOG("current undo data now \"" << u->strdata << '"');
		break;
	case DEL:
		len = strlen(u->strdata) + 2;
		assert(len > 2);
		if (fs->current_x == u->begin) {
			/* They're deleting */
			if (!u->xflags) {
				u->xflags = UNdel_del;
			} else if (u->xflags != UNdel_del) {
				add_undo(action);
				return;
			}
			data = charalloc(len);
			strcpy(data, u->strdata);
			data[len-2] = fs->current->data[fs->current_x];
			data[len-1] = '\0';
			free(u->strdata);
			u->strdata = data;
		} else if (fs->current_x == u->begin - 1) {
			/* They're backspacing */
			if (!u->xflags) {
				u->xflags = UNdel_backspace;
			} else if (u->xflags != UNdel_backspace) {
				add_undo(action);
				return;
			}
			data = charalloc(len);
			data[0] = fs->current->data[fs->current_x];
			strcpy(&data[1], u->strdata);
			free(u->strdata);
			u->strdata = data;
			u->begin--;
		} else {
			/* They deleted something else on the line */
			add_undo(DEL);
			return;
		}
		DEBUG_LOG("current undo data now \"" << u->strdata << '"' << std::endl << "u->begin = " << u->begin);
		break;
	case CUT:
		if (!cutbuffer) {
			break;
		}
		if (u->cutbuffer) {
			free(u->cutbuffer);
		}
		u->cutbuffer = copy_filestruct(cutbuffer);
		/* Compute cutbottom for the uncut using out copy */
		for (u->cutbottom = u->cutbuffer; u->cutbottom->next != NULL; u->cutbottom = u->cutbottom->next) {
			;
		}
		break;
	case REPLACE:
	case UNCUT:
		add_undo(action);
		break;
	case INSERT:
		u->mark_begin_lineno = openfile->current->lineno;
		break;
	case SPLIT:
		/* This will only be called if we made a completely new line,
		   and as such we should note that so we can destroy it later */
		u->xflags = UNsplit_madenew;
		break;
	case UNSPLIT:
		/* These cases are handled by the earlier check for a new line and action */
	case ENTER:
	case OTHER:
		break;
	}

	DEBUG_LOG("Done in udpate_undo (type was " << action << ')');
	if (fs->last_action != action) {
		DEBUG_LOG("Starting add_undo for new action as it does not match last_action");
		add_undo(action);
	}
	fs->last_action = action;
}

/* Unset the prepend_wrap flag.  We need to do this as soon as we do
 * something other than type text. */
void wrap_reset(void)
{
	prepend_wrap = FALSE;
}

/* We wrap the given line.  Precondition: we assume the cursor has been
 * moved forward since the last typed character.  Return TRUE if we
 * wrapped, and FALSE otherwise. */
bool do_wrap(filestruct *line, bool undoing)
{
	size_t line_len;
	/* The length of the line we wrap. */
	ssize_t wrap_loc;
	/* The index of line->data where we wrap. */
	const char *indent_string = NULL;
	/* Indentation to prepend to the new line. */
	size_t indent_len = 0;
	/* The length of indent_string. */
	const char *after_break;
	/* The text after the wrap point. */
	size_t after_break_len;
	/* The length of after_break. */
	bool prepending = FALSE;
	/* Do we prepend to the next line? */
	const char *next_line = NULL;
	/* The next line, minus indentation. */
	size_t next_line_len = 0;
	/* The length of next_line. */
	char *new_line = NULL;
	/* The line we create. */
	size_t new_line_len = 0;
	/* The eventual length of new_line. */

	/* There are three steps.  First, we decide where to wrap.  Then, we
	 * create the new wrap line.  Finally, we clean up. */

	/* Step 1, finding where to wrap.  We are going to add a new line
	 * after a blank character.  In this step, we call break_line() to
	 * get the location of the last blank we can break the line at, and
	 * set wrap_loc to the location of the character after it, so that
	 * the blank is preserved at the end of the line.
	 *
	 * If there is no legal wrap point, or we reach the last character
	 * of the line while trying to find one, we should return without
	 * wrapping.  Note that if autoindent is turned on, we don't break
	 * at the end of it! */
	assert(line != NULL && line->data != NULL);

	/* Save the length of the line. */
	line_len = strlen(line->data);

	/* Find the last blank where we can break the line. */
	wrap_loc = break_line(line->data, fill, FALSE);

	/* If we couldn't break the line, or we've reached the end of it, we
	 * don't wrap. */
	if (wrap_loc == -1 || line->data[wrap_loc] == '\0') {
		return FALSE;
	}

	/* Otherwise, move forward to the character just after the blank. */
	wrap_loc += move_mbright(line->data + wrap_loc, 0);

	/* If we've reached the end of the line, we don't wrap. */
	if (line->data[wrap_loc] == '\0') {
		return FALSE;
	}

	if (!undoing) {
		add_undo(SPLIT);
	}

	/* If autoindent is turned on, and we're on the character just after
	 * the indentation, we don't wrap. */
	if (ISSET(AUTOINDENT)) {
		/* Get the indentation of this line. */
		indent_string = line->data;
		indent_len = indent_length(indent_string);

		if (wrap_loc == indent_len) {
			return FALSE;
		}
	}

	/* Step 2, making the new wrap line.  It will consist of indentation
	 * followed by the text after the wrap point, optionally followed by
	 * a space (if the text after the wrap point doesn't end in a blank)
	 * and the text of the next line, if they can fit without wrapping,
	 * the next line exists, and the prepend_wrap flag is set. */

	/* after_break is the text that will be wrapped to the next line. */
	after_break = line->data + wrap_loc;
	after_break_len = line_len - wrap_loc;

	assert(strlen(after_break) == after_break_len);

	/* We prepend the wrapped text to the next line, if the prepend_wrap
	 * flag is set, there is a next line, and prepending would not make
	 * the line too long. */
	if (prepend_wrap && line != openfile->filebot) {
		const char *end = after_break + move_mbleft(after_break, after_break_len);

		/* If after_break doesn't end in a blank, make sure it ends in a
		 * space. */
		if (!is_blank_mbchar(end)) {
			line_len++;
			line->data = charealloc(line->data, line_len + 1);
			line->data[line_len - 1] = ' ';
			line->data[line_len] = '\0';
			after_break = line->data + wrap_loc;
			after_break_len++;
			openfile->totsize++;
		}

		next_line = line->next->data;
		next_line_len = strlen(next_line);

		if (after_break_len + next_line_len <= fill) {
			prepending = TRUE;
			new_line_len += next_line_len;
		}
	}

	/* new_line_len is now the length of the text that will be wrapped
	 * to the next line, plus (if we're prepending to it) the length of
	 * the text of the next line. */
	new_line_len += after_break_len;

	if (ISSET(AUTOINDENT)) {
		if (prepending) {
			/* If we're prepending, the indentation will come from the next line. */
			indent_string = next_line;
			indent_len = indent_length(indent_string);
			next_line += indent_len;
		} else {
			/* Otherwise, it will come from this line, in which case
			 * we should increase new_line_len to make room for it. */
			new_line_len += indent_len;
			openfile->totsize += mbstrnlen(indent_string, indent_len);
		}
	}

	/* Now we allocate the new line and copy the text into it. */
	new_line = charalloc(new_line_len + 1);
	new_line[0] = '\0';

	if (ISSET(AUTOINDENT)) {
		/* Copy the indentation. */
		strncpy(new_line, indent_string, indent_len);
		new_line[indent_len] = '\0';
		new_line_len += indent_len;
	}

	/* Copy all the text after the wrap point of the current line. */
	strcat(new_line, after_break);

	/* Break the current line at the wrap point. */
	null_at(&line->data, wrap_loc);

	if (prepending) {
		if (!undoing) {
			update_undo(SPLIT);
		}
		/* If we're prepending, copy the text from the next line, minus
		 * the indentation that we already copied above. */
		strcat(new_line, next_line);

		free(line->next->data);
		line->next->data = new_line;

		/* If the NO_NEWLINES flag isn't set, and text has been added to
		 * the magicline, make a new magicline. */
		if (!ISSET(NO_NEWLINES) && openfile->filebot->data[0] != '\0') {
			new_magicline();
		}
	} else {
		/* Otherwise, make a new line and copy the text after where we
		 * broke this line to the beginning of the new line. */
		splice_node(openfile->current, make_new_node(openfile->current), openfile->current->next);

		/* If the current line is the last line of the file, move the
		 * last line of the file down to the next line. */
		if (openfile->filebot == openfile->current) {
			openfile->filebot = openfile->current->next;
		}

		openfile->current->next->data = new_line;

		openfile->totsize++;
	}

	/* Step 3, clean up.  Reposition the cursor and mark, and do some
	 * other sundry things. */

	/* Set the prepend_wrap flag, so that later wraps of this line will
	 * be prepended to the next line. */
	prepend_wrap = TRUE;

	/* Each line knows its number.  We recalculate these if we inserted
	 * a new line. */
	if (!prepending) {
		renumber(line);
	}

	/* If the cursor was after the break point, we must move it.  We
	 * also clear the prepend_wrap flag in this case. */
	if (openfile->current_x > wrap_loc) {
		prepend_wrap = FALSE;

		openfile->current = openfile->current->next;
		openfile->current_x -= wrap_loc - indent_len;
		openfile->placewewant = xplustabs();
	}

	/* If the mark was on this line after the wrap point, we move it
	 * down.  If it was on the next line and we prepended to that line,
	 * we move it right. */
	if (openfile->mark_set) {
		if (openfile->mark_begin == line && openfile->mark_begin_x > wrap_loc) {
			openfile->mark_begin = line->next;
			openfile->mark_begin_x -= wrap_loc - indent_len + 1;
		} else if (prepending && openfile->mark_begin == line->next) {
			openfile->mark_begin_x += after_break_len;
		}
	}

	return TRUE;
}

/* We are trying to break a chunk off line.  We find the last blank such
 * that the display length to there is at most (goal + 1).  If there is
 * no such blank, then we find the first blank.  We then take the last
 * blank in that group of blanks.  The terminating '\0' counts as a
 * blank, as does a '\n' if newline is TRUE. */
ssize_t break_line(const char *line, ssize_t goal, bool newln)
{
	ssize_t blank_loc = -1;
	/* Current tentative return value.  Index of the last blank we
	 * found with short enough display width.  */
	ssize_t cur_loc = 0;
	/* Current index in line. */
	size_t cur_pos = 0;
	/* Current column position in line. */
	int line_len;

	assert(line != NULL);

	while (*line != '\0' && goal >= cur_pos) {
		line_len = parse_mbchar(line, NULL, &cur_pos);

		if (is_blank_mbchar(line) || (newln && *line == '\n')) {
			blank_loc = cur_loc;

			if (newln && *line == '\n') {
				break;
			}
		}

		line += line_len;
		cur_loc += line_len;
	}

	if (goal >= cur_pos) {
		/* In fact, the whole line displays shorter than goal. */
		return cur_loc;
	}

	if (newln && blank_loc <= 0) {
		/* If blank was not found or was found only first character,
		 * force line break. */
		cur_loc -= line_len;
		return cur_loc;
	}

	if (blank_loc == -1) {
		/* No blank was found that was short enough. */
		bool found_blank = FALSE;
		ssize_t found_blank_loc = 0;

		while (*line != '\0') {
			line_len = parse_mbchar(line, NULL, NULL);

			if (is_blank_mbchar(line) || (newln && *line == '\n')) {
				if (!found_blank) {
					found_blank = TRUE;
				}
				found_blank_loc = cur_loc;
			} else if (found_blank) {
				return found_blank_loc;
			}

			line += line_len;
			cur_loc += line_len;
		}

		return -1;
	}

	/* Move to the last blank after blank_loc, if there is one. */
	line -= cur_loc;
	line += blank_loc;
	line_len = parse_mbchar(line, NULL, NULL);
	line += line_len;

	while (*line != '\0' && (is_blank_mbchar(line) || (newln && *line == '\n'))) {
		if (newln && *line == '\n') {
			break;
		}

		line_len = parse_mbchar(line, NULL, NULL);

		line += line_len;
		blank_loc += line_len;
	}

	return blank_loc;
}

/* The "indentation" of a line is the whitespace between the quote part
 * and the non-whitespace of the line. */
size_t indent_length(const char *line)
{
	size_t len = 0;
	char *blank_mb;
	int blank_mb_len;

	assert(line != NULL);

	blank_mb = charalloc(mb_cur_max());

	while (*line != '\0') {
		blank_mb_len = parse_mbchar(line, blank_mb, NULL);

		if (!is_blank_mbchar(blank_mb)) {
			break;
		}

		line += blank_mb_len;
		len += blank_mb_len;
	}

	free(blank_mb);

	return len;
}

#ifdef ENABLE_SPELLER
/* A word is misspelled in the file.  Let the user replace it.  We
 * return FALSE if the user cancels. */
bool do_int_spell_fix(const char *word)
{
	char *save_search, *save_replace;
	size_t match_len, current_x_save = openfile->current_x;
	size_t pww_save = openfile->placewewant;
	filestruct *edittop_save = openfile->edittop;
	filestruct *current_save = openfile->current;
	/* Save where we are. */
	bool canceled = FALSE;
	/* The return value. */
	bool case_sens_set = ISSET(CASE_SENSITIVE);
	bool backwards_search_set = ISSET(BACKWARDS_SEARCH);
	bool regexp_set = ISSET(USE_REGEXP);
	bool old_mark_set = openfile->mark_set;
	bool added_magicline = FALSE;
	/* Whether we added a magicline after filebot. */
	bool right_side_up = FALSE;
	/* TRUE if (mark_begin, mark_begin_x) is the top of the mark,
	 * FALSE if (current, current_x) is. */
	filestruct *top, *bot;
	size_t top_x, bot_x;

	/* Make sure spell-check is case sensitive. */
	SET(CASE_SENSITIVE);

	/* Make sure spell-check goes forward only. */
	UNSET(BACKWARDS_SEARCH);
	/* Make sure spell-check doesn't use regular expressions. */
	UNSET(USE_REGEXP);

	/* Save the current search/replace strings. */
	search_init_globals();
	save_search = last_search;
	save_replace = last_replace;

	/* Set the search/replace strings to the misspelled word. */
	last_search = mallocstrcpy(NULL, word);
	last_replace = mallocstrcpy(NULL, word);

	if (old_mark_set) {
		/* If the mark is on, partition the filestruct so that it
		 * contains only the marked text; if the NO_NEWLINES flag isn't
		 * set, keep track of whether the text will have a magicline
		 * added when we're done correcting misspelled words; and
		 * turn the mark off. */
		mark_order((const filestruct **)&top, &top_x, (const filestruct **)&bot, &bot_x, &right_side_up);
		filepart = partition_filestruct(top, top_x, bot, bot_x);
		if (!ISSET(NO_NEWLINES)) {
			added_magicline = (openfile->filebot->data[0] != '\0');
		}
		openfile->mark_set = FALSE;
	}

	/* Start from the top of the file. */
	openfile->edittop = openfile->fileage;
	openfile->current = openfile->fileage;
	openfile->current_x = (size_t)-1;
	openfile->placewewant = 0;

	/* Find the first whole occurrence of word. */
	findnextstr_wrap_reset();
	while (findnextstr(TRUE, FALSE, openfile->fileage, 0, word, &match_len)) {
		if (is_whole_word(openfile->current_x, openfile->current->data, word)) {
			size_t xpt = xplustabs();
			char *exp_word = display_string(openfile->current->data, xpt, strnlenpt(openfile->current->data, openfile->current_x + match_len) - xpt, FALSE);

			edit_refresh();

			do_replace_highlight(TRUE, exp_word);

			/* Allow all instances of the word to be corrected. */
			std::shared_ptr<Key> key;
			canceled = (do_prompt(FALSE, TRUE, MSPELL, key, word, NULL, edit_refresh, _("Edit a replacement")) == PROMPT_ABORTED);

			do_replace_highlight(FALSE, exp_word);

			free(exp_word);

			if (!canceled && strcmp(word, answer) != 0) {
				openfile->current_x--;
				do_replace_loop(TRUE, &canceled, openfile->current, &openfile->current_x, word);
			}

			break;
		}
	}

	if (old_mark_set) {
		/* If the mark was on, the NO_NEWLINES flag isn't set, and we
		 * added a magicline, remove it now. */
		if (!ISSET(NO_NEWLINES) && added_magicline) {
			remove_magicline();
		}

		/* Put the beginning and the end of the mark at the beginning
		 * and the end of the spell-checked text. */
		if (openfile->fileage == openfile->filebot) {
			bot_x += top_x;
		}
		if (right_side_up) {
			openfile->mark_begin_x = top_x;
			current_x_save = bot_x;
		} else {
			current_x_save = top_x;
			openfile->mark_begin_x = bot_x;
		}

		/* Unpartition the filestruct so that it contains all the text
		 * again, and turn the mark back on. */
		unpartition_filestruct(&filepart);
		openfile->mark_set = TRUE;
	}

	/* Restore the search/replace strings. */
	free(last_search);
	last_search = save_search;
	free(last_replace);
	last_replace = save_replace;

	/* Restore where we were. */
	openfile->edittop = edittop_save;
	openfile->current = current_save;
	openfile->current_x = current_x_save;
	openfile->placewewant = pww_save;

	/* Restore case sensitivity setting. */
	if (!case_sens_set) {
		UNSET(CASE_SENSITIVE);
	}

	/* Restore search/replace direction. */
	if (backwards_search_set) {
		SET(BACKWARDS_SEARCH);
	}
	/* Restore regular expression usage setting. */
	if (regexp_set) {
		SET(USE_REGEXP);
	}

	return !canceled;
}

/* Internal (integrated) spell checking using the spell program,
 * filtered through the sort and uniq programs.  Return NULL for normal
 * termination, and the error string otherwise. */
const char *do_int_speller(const char *tempfile_name)
{
	char *read_buff, *read_buff_ptr, *read_buff_word;
	size_t pipe_buff_size, read_buff_size, read_buff_read, bytesread;
	int spell_fd[2], sort_fd[2], uniq_fd[2], tempfile_fd = -1;
	pid_t pid_spell, pid_sort, pid_uniq;
	int spell_status, sort_status, uniq_status;

	/* Create all three pipes up front. */
	if (pipe(spell_fd) == -1 || pipe(sort_fd) == -1 || pipe(uniq_fd) == -1) {
		return _("Could not create pipe");
	}

	statusbar(_("Creating misspelled word list, please wait..."));

	/* A new process to run spell in. */
	if ((pid_spell = fork()) == 0) {
		/* Child continues (i.e. future spell process). */
		close(spell_fd[0]);

		/* Replace the standard input with the temp file. */
		if ((tempfile_fd = open(tempfile_name, O_RDONLY)) == -1) {
			goto close_pipes_and_exit;
		}

		if (dup2(tempfile_fd, STDIN_FILENO) != STDIN_FILENO) {
			goto close_pipes_and_exit;
		}

		close(tempfile_fd);

		/* Send spell's standard output to the pipe. */
		if (dup2(spell_fd[1], STDOUT_FILENO) != STDOUT_FILENO) {
			goto close_pipes_and_exit;
		}

		close(spell_fd[1]);

		/* Start the spell program; we are using $PATH. */
		execlp("spell", "spell", NULL);

		/* This should not be reached if spell is found. */
		exit(1);
	}

	/* Parent continues here. */
	close(spell_fd[1]);

	/* A new process to run sort in. */
	if ((pid_sort = fork()) == 0) {
		/* Child continues (i.e. future spell process).  Replace the
		 * standard input with the standard output of the old pipe. */
		if (dup2(spell_fd[0], STDIN_FILENO) != STDIN_FILENO) {
			goto close_pipes_and_exit;
		}

		close(spell_fd[0]);

		/* Send sort's standard output to the new pipe. */
		if (dup2(sort_fd[1], STDOUT_FILENO) != STDOUT_FILENO) {
			goto close_pipes_and_exit;
		}

		close(sort_fd[1]);

		/* Start the sort program.  Use -f to remove mixed case.  If
		 * this isn't portable, let me know. */
		execlp("sort", "sort", "-f", NULL);

		/* This should not be reached if sort is found. */
		exit(1);
	}

	close(spell_fd[0]);
	close(sort_fd[1]);

	/* A new process to run uniq in. */
	if ((pid_uniq = fork()) == 0) {
		/* Child continues (i.e. future uniq process).  Replace the
		 * standard input with the standard output of the old pipe. */
		if (dup2(sort_fd[0], STDIN_FILENO) != STDIN_FILENO) {
			goto close_pipes_and_exit;
		}

		close(sort_fd[0]);

		/* Send uniq's standard output to the new pipe. */
		if (dup2(uniq_fd[1], STDOUT_FILENO) != STDOUT_FILENO) {
			goto close_pipes_and_exit;
		}

		close(uniq_fd[1]);

		/* Start the uniq program; we are using PATH. */
		execlp("uniq", "uniq", NULL);

		/* This should not be reached if uniq is found. */
		exit(1);
	}

	close(sort_fd[0]);
	close(uniq_fd[1]);

	/* The child process was not forked successfully. */
	if (pid_spell < 0 || pid_sort < 0 || pid_uniq < 0) {
		close(uniq_fd[0]);
		return _("Could not fork");
	}

	/* Get the system pipe buffer size. */
	if ((pipe_buff_size = fpathconf(uniq_fd[0], _PC_PIPE_BUF)) < 1) {
		close(uniq_fd[0]);
		return _("Could not get size of pipe buffer");
	}

	/* Read in the returned spelling errors. */
	read_buff_read = 0;
	read_buff_size = pipe_buff_size + 1;
	read_buff = read_buff_ptr = charalloc(read_buff_size);

	while ((bytesread = read(uniq_fd[0], read_buff_ptr, pipe_buff_size)) > 0) {
		read_buff_read += bytesread;
		read_buff_size += pipe_buff_size;
		read_buff = read_buff_ptr = charealloc(read_buff, read_buff_size);
		read_buff_ptr += read_buff_read;
	}

	*read_buff_ptr = '\0';
	close(uniq_fd[0]);

	/* Process the spelling errors. */
	read_buff_word = read_buff_ptr = read_buff;

	while (*read_buff_ptr != '\0') {
		if ((*read_buff_ptr == '\r') || (*read_buff_ptr == '\n')) {
			*read_buff_ptr = '\0';
			if (read_buff_word != read_buff_ptr) {
				if (!do_int_spell_fix(read_buff_word)) {
					read_buff_word = read_buff_ptr;
					break;
				}
			}
			read_buff_word = read_buff_ptr + 1;
		}
		read_buff_ptr++;
	}

	/* Special case: the last word doesn't end with '\r' or '\n'. */
	if (read_buff_word != read_buff_ptr) {
		do_int_spell_fix(read_buff_word);
	}

	free(read_buff);
	search_replace_abort();
	edit_refresh_needed = TRUE;

	/* Process the end of the spell process. */
	waitpid(pid_spell, &spell_status, 0);
	waitpid(pid_sort, &sort_status, 0);
	waitpid(pid_uniq, &uniq_status, 0);

	if (WIFEXITED(spell_status) == 0 || WEXITSTATUS(spell_status)) {
		return _("Error invoking \"spell\"");
	}

	if (WIFEXITED(sort_status)  == 0 || WEXITSTATUS(sort_status)) {
		return _("Error invoking \"sort -f\"");
	}

	if (WIFEXITED(uniq_status) == 0 || WEXITSTATUS(uniq_status)) {
		return _("Error invoking \"uniq\"");
	}

	/* Otherwise... */
	return NULL;

close_pipes_and_exit:
	/* Don't leak any handles. */
	close(tempfile_fd);
	close(spell_fd[0]);
	close(spell_fd[1]);
	close(sort_fd[0]);
	close(sort_fd[1]);
	close(uniq_fd[0]);
	close(uniq_fd[1]);
	exit(1);
}

/* External (alternate) spell checking.  Return NULL for normal
 * termination, and the error string otherwise. */
const char *do_alt_speller(char *tempfile_name)
{
	int alt_spell_status;
	size_t current_x_save = openfile->current_x;
	size_t pww_save = openfile->placewewant;
	ssize_t current_y_save = openfile->current_y;
	ssize_t lineno_save = openfile->current->lineno;
	pid_t pid_spell;
	char *ptr;
	static int arglen = 3;
	static char **spellargs = NULL;
	bool old_mark_set = openfile->mark_set;
	bool added_magicline = FALSE;
	/* Whether we added a magicline after filebot. */
	bool right_side_up = FALSE;
	/* TRUE if (mark_begin, mark_begin_x) is the top of the mark,
	 * FALSE if (current, current_x) is. */
	filestruct *top, *bot;
	size_t top_x, bot_x;
	ssize_t mb_lineno_save = 0;
	/* We're going to close the current file, and open the output of
	 * the alternate spell command.  The line that mark_begin points
	 * to will be freed, so we save the line number and restore it
	 * afterwards. */
	size_t totsize_save = openfile->totsize;
	/* Our saved value of totsize, used when we spell-check a marked selection. */

	if (old_mark_set) {
		/* If the mark is on, save the number of the line it starts on,
		 * and then turn the mark off. */
		mb_lineno_save = openfile->mark_begin->lineno;
		openfile->mark_set = FALSE;
	}

	if (openfile->totsize == 0) {
		statusbar(_("Finished checking spelling"));
		return NULL;
	}

	endwin();

	/* Set up an argument list to pass execvp(). */
	if (spellargs == NULL) {
		spellargs = (char **)nmalloc(arglen * sizeof(char *));

		spellargs[0] = strtok(alt_speller, " ");
		while ((ptr = strtok(NULL, " ")) != NULL) {
			arglen++;
			spellargs = (char **)nrealloc(spellargs, arglen * sizeof(char *));
			spellargs[arglen - 3] = ptr;
		}
		spellargs[arglen - 1] = NULL;
	}
	spellargs[arglen - 2] = tempfile_name;

	/* Start a new process for the alternate speller. */
	if ((pid_spell = fork()) == 0) {
		/* Start alternate spell program; we are using $PATH. */
		execvp(spellargs[0], spellargs);

		/* Should not be reached, if alternate speller is found!!! */
		exit(1);
	}

	/* If we couldn't fork, get out. */
	if (pid_spell < 0) {
		return _("Could not fork");
	}

	/* Don't handle a pending SIGWINCH until the alternate spell checker
	 * is finished and we've loaded the spell-checked file back in. */
	allow_pending_sigwinch(FALSE);

	/* Wait for the alternate spell checker to finish. */
	wait(&alt_spell_status);

	/* Reenter curses mode. */
	doupdate();

	/* Restore the terminal to its previous state. */
	terminal_init();

	/* Turn the cursor back on for sure. */
	curs_set(1);

	/* The screen might have been resized.  If it has, reinitialize all
	 * the windows based on the new screen dimensions. */
	window_init();

	if (!WIFEXITED(alt_spell_status) || WEXITSTATUS(alt_spell_status) != 0) {
		char *alt_spell_error;
		char *invoke_error = _("Error invoking \"%s\"");

#ifndef PINOTO_TINY
		/* Turn the mark back on if it was on before. */
		openfile->mark_set = old_mark_set;
#endif

		alt_spell_error = charalloc(strlen(invoke_error) + strlen(alt_speller) + 1);
		sprintf(alt_spell_error, invoke_error, alt_speller);
		return alt_spell_error;
	}

	if (old_mark_set) {
		/* If the mark is on, partition the filestruct so that it
		 * contains only the marked text; if the NO_NEWLINES flag isn't
		 * set, keep track of whether the text will have a magicline
		 * added when we're done correcting misspelled words; and
		 * turn the mark off. */
		mark_order((const filestruct **)&top, &top_x, (const filestruct **)&bot, &bot_x, &right_side_up);
		filepart = partition_filestruct(top, top_x, bot, bot_x);
		if (!ISSET(NO_NEWLINES)) {
			added_magicline = (openfile->filebot->data[0] != '\0');
		}

		/* Get the number of characters in the marked text, and subtract
		 * it from the saved value of totsize. */
		totsize_save -= get_totsize(top, bot);
	}

	/* Replace the text of the current buffer with the spell-checked text. */
	replace_buffer(tempfile_name);

	if (old_mark_set) {
		filestruct *top_save = openfile->fileage;

		/* If the mark was on, the NO_NEWLINES flag isn't set, and we
		 * added a magicline, remove it now. */
		if (!ISSET(NO_NEWLINES) && added_magicline) {
			remove_magicline();
		}

		/* Put the beginning and the end of the mark at the beginning
		 * and the end of the spell-checked text. */
		if (openfile->fileage == openfile->filebot) {
			bot_x += top_x;
		}
		if (right_side_up) {
			openfile->mark_begin_x = top_x;
			current_x_save = bot_x;
		} else {
			current_x_save = top_x;
			openfile->mark_begin_x = bot_x;
		}

		/* Unpartition the filestruct so that it contains all the text
		 * again.  Note that we've replaced the marked text originally
		 * in the partition with the spell-checked marked text in the
		 * temp file. */
		unpartition_filestruct(&filepart);

		/* Renumber starting with the beginning line of the old
		 * partition.  Also add the number of characters in the
		 * spell-checked marked text to the saved value of totsize, and
		 * then make that saved value the actual value. */
		renumber(top_save);
		totsize_save += openfile->totsize;
		openfile->totsize = totsize_save;

		/* Assign mark_begin to the line where the mark began before. */
		do_gotopos(mb_lineno_save, openfile->mark_begin_x, current_y_save, 0);
		openfile->mark_begin = openfile->current;

		/* Assign mark_begin_x to the location in mark_begin where the
		 * mark began before, adjusted for any shortening of the line. */
		openfile->mark_begin_x = openfile->current_x;

		/* Turn the mark back on. */
		openfile->mark_set = TRUE;
	}

	/* Go back to the old position, and mark the file as modified. */
	do_gotopos(lineno_save, current_x_save, current_y_save, pww_save);
	set_modified();

	/* Handle a pending SIGWINCH again. */
	allow_pending_sigwinch(TRUE);

	return NULL;
}

/* Spell check the current file.  If an alternate spell checker is
 * specified, use it.  Otherwise, use the internal spell checker. */
void do_spell(void)
{
	bool status;
	FILE *temp_file;
	char *temp = safe_tempfile(&temp_file);
	const char *spell_msg;

	if (temp == NULL) {
		statusbar(_("Error writing temp file: %s"), strerror(errno));
		return;
	}

	status = openfile->mark_set ? write_marked_file(temp, temp_file, TRUE, OVERWRITE) : write_file(temp, temp_file, TRUE, OVERWRITE, FALSE);

	if (!status) {
		statusbar(_("Error writing temp file: %s"), strerror(errno));
		free(temp);
		return;
	}

	spell_msg = (alt_speller != NULL) ? do_alt_speller(temp) : do_int_speller(temp);
	unlink(temp);
	free(temp);

	currmenu = MMAIN;

	/* If the spell-checker printed any error messages onscreen, make
	 * sure that they're cleared off. */
	total_refresh();

	if (spell_msg != NULL) {
		if (errno == 0) {
			/* Don't display an error message of "Success". */
			statusbar(_("Spell checking failed: %s"), spell_msg);
		} else {
			statusbar(_("Spell checking failed: %s: %s"), spell_msg, strerror(errno));
		}
	} else {
		statusbar(_("Finished checking spelling"));
	}
}
#endif /* ENABLE_SPELLER */

/* Run linter.  Based on alt-speller code.  Return NULL for normal
 * termination, and the error string otherwise. */
void do_linter(void)
{
	char *read_buff, *read_buff_ptr, *read_buff_word, *ptr;
	size_t pipe_buff_size, read_buff_size, read_buff_read, bytesread;
	size_t parsesuccess = 0;
	int lint_fd[2];
	pid_t pid_lint;
	int lint_status;
	static int arglen = 3;
	static char **lintargs = NULL;
	char *lintcopy;
	char *convendptr = NULL;
	const sc *s;
	LintMessages lints;

	if (!openfile->syntax || openfile->syntax->linter == "") {
		statusbar(_("No linter defined for this file!"));
		return;
	}

	if (openfile->modified) {
		int i = do_yesno_prompt(FALSE, _("Save modified buffer before linting?"));

		if (i == 1) {
			if (do_writeout(FALSE) != TRUE) {
				return;
			}
		}
	}

	lintcopy = mallocstrcpy(NULL, openfile->syntax->linter.c_str());
	/* Create pipe up front. */
	if (pipe(lint_fd) == -1) {
		statusbar(_("Could not create pipe"));
		return;
	}

	statusbar(_("Invoking linter, please wait"));

	/* Set up an argument list to pass execvp(). */
	if (lintargs == NULL) {
		lintargs = (char **)nmalloc(arglen * sizeof(char *));

		lintargs[0] = strtok(lintcopy, " ");
		while ((ptr = strtok(NULL, " ")) != NULL) {
			arglen++;
			lintargs = (char **)nrealloc(lintargs, arglen * sizeof(char *));
			lintargs[arglen - 3] = ptr;
		}
		lintargs[arglen - 1] = NULL;
	}
	lintargs[arglen - 2] = openfile->filename;

	/* A new process to run linter. */
	if ((pid_lint = fork()) == 0) {

		/* Child continues (i.e. future spell process). */
		close(lint_fd[0]);

		/* Send linter's standard output/err to the pipe. */
		if (dup2(lint_fd[1], STDOUT_FILENO) != STDOUT_FILENO) {
			exit(1);
		}
		if (dup2(lint_fd[1], STDERR_FILENO) != STDERR_FILENO) {
			exit(1);
		}

		close(lint_fd[1]);

		/* Start the linter program; we are using $PATH. */
		execvp(lintargs[0], lintargs);

		/* This should not be reached if linter is found. */
		exit(1);
	}

	/* Parent continues here. */
	close(lint_fd[1]);

	/* The child process was not forked successfully. */
	if (pid_lint < 0) {
		close(lint_fd[0]);
		statusbar(_("Could not fork"));
		return;
	}

	/* Get the system pipe buffer size. */
	if ((pipe_buff_size = fpathconf(lint_fd[0], _PC_PIPE_BUF)) < 1) {
		close(lint_fd[0]);
		statusbar(_("Could not get size of pipe buffer"));
		return;
	}

	/* Read in the returned spelling errors. */
	read_buff_read = 0;
	read_buff_size = pipe_buff_size + 1;
	read_buff = read_buff_ptr = charalloc(read_buff_size);

	while ((bytesread = read(lint_fd[0], read_buff_ptr, pipe_buff_size)) > 0) {
		DEBUG_LOG("do_linter(): read " << bytesread << '(' << read_buff_ptr << ')');
		read_buff_read += bytesread;
		read_buff_size += pipe_buff_size;
		read_buff = read_buff_ptr = charealloc(read_buff, read_buff_size);
		read_buff_ptr += read_buff_read;
	}

	*read_buff_ptr = '\0';
	close(lint_fd[0]);

	DEBUG_LOG("do_lint(): Raw output: " << read_buff);

	/* Process output. */
	read_buff_word = read_buff_ptr = read_buff;

	while (*read_buff_ptr != '\0') {
		if ((*read_buff_ptr == '\r') || (*read_buff_ptr == '\n')) {
			*read_buff_ptr = '\0';
			if (read_buff_word != read_buff_ptr) {
				char *filename = NULL, *linestr = NULL, *maybecol = NULL;
				char *message = mallocstrcpy(NULL, read_buff_word);

				/* At the moment we're assuming the following formats:
				 * filenameorcategory:line:column:message (e.g. splint)
				 * filenameorcategory:line:message        (e.g. pyflakes)
				 * filenameorcategory:line,col:message    (e.g. pylint)
				 * This could be turnes into some scanf() based parser but ugh.
			  */
				if ((filename = strtok(read_buff_word, ":")) != NULL) {
					if ((linestr = strtok(NULL, ":")) != NULL) {
						if ((maybecol = strtok(NULL, ":")) != NULL) {
							ssize_t tmplineno = 0, tmpcolno = 0;
							char *tmplinecol;

							tmplineno = strtol(linestr, NULL, 10);
							if (tmplineno <= 0) {
								read_buff_ptr++;
								free(message);
								continue;
							}

							tmpcolno = strtol(maybecol, &convendptr, 10);
							if (*convendptr != '\0') {
								/* Prev field might still be line,col format */
								strtok(linestr, ",");
								if ((tmplinecol = strtok(NULL, ",")) != NULL) {
									tmpcolno = strtol(tmplinecol, NULL, 10);
								}
							}

							DEBUG_LOG("do_lint(): Successful parse! " << tmplineno << ':' << tmpcolno << ':' << message);

							/* Nice we have a lint message we can use */
							parsesuccess++;
							LintMessage lint;
							lint.text     = std::string(message);
							lint.lineno   = tmplineno;
							lint.colno    = tmpcolno;
							lint.filename = std::string(filename);
							lints.push_back(lint);
						}
					}
				} else {
					free(message);
				}
			}
			read_buff_word = read_buff_ptr + 1;
		}
		read_buff_ptr++;
	}

	/* Process the end of the lint process. */
	waitpid(pid_lint, &lint_status, 0);

	free(read_buff);

	if (parsesuccess == 0) {
		statusbar(_("Got 0 parsable lines from command: %s"), openfile->syntax->linter.c_str());
		return;
	}

	currmenu = MLINTER;
	bottombars(MLINTER);

	auto curr_lint = lints.begin();
	LintMessages::iterator last_lint;

	while (1) {
		ssize_t tmpcol = 1;

		if (curr_lint->colno > 0) {
			tmpcol = curr_lint->colno;
		}

		if (last_lint != curr_lint) {
			struct stat lintfileinfo;

			new_lint_loop:
			if (stat(curr_lint->filename.c_str(), &lintfileinfo) != -1) {
				if (openfile->current_stat->st_ino != lintfileinfo.st_ino) {
					openfilestruct *tmpof = openfile;
					while (tmpof != openfile->next) {
						if (tmpof->current_stat->st_ino == lintfileinfo.st_ino) {
							break;
						}
						tmpof = tmpof->next;
					}
					if (tmpof->current_stat->st_ino != lintfileinfo.st_ino) {
						char *msg = charalloc(1024 + curr_lint->filename.length());
						int i;

						sprintf(msg, _("This message is for unopened file %s, open it in a new buffer?"), curr_lint->filename.c_str());
						i = do_yesno_prompt(FALSE, msg);
						free(msg);
						if (i == 1) {
							SET(MULTIBUFFER);
							open_buffer(curr_lint->filename.c_str(), FALSE);
						} else {
							auto dontwantfile = curr_lint->filename;

							while (++curr_lint != lints.end() && curr_lint->filename == dontwantfile) {
								// nothing
							}
							if (curr_lint == lints.end()) {
								statusbar("No more errors in un-opened filed, cancelling");
								break;
							} else {
								goto new_lint_loop;
							}
						}
					} else {
						openfile = tmpof;
					}
				}
			}
			do_gotolinecolumn(curr_lint->lineno, tmpcol, FALSE, FALSE, FALSE, FALSE);
			titlebar(NULL);
			edit_refresh();
			statusbar(curr_lint->text.c_str());
			bottombars(MLINTER);
		}

		Key kbinput = get_kbinput(bottomwin);
		s = get_shortcut(currmenu, kbinput);
		last_lint = curr_lint;

		if (!s) {
			continue;
		} else if (s->scfunc == do_cancel) {
			break;
		} else if (s->scfunc == do_help_void) {
			do_help_void();
		} else if (s->scfunc == do_page_down) {
			if (curr_lint == lints.end()) {
				statusbar(_("At last message"));
				continue;
			} else {
				++curr_lint;
			}
		} else if (s->scfunc == do_page_up) {
			if (curr_lint == lints.begin()) {
				statusbar(_("At first message"));
				continue;
			} else {
				--curr_lint;
			}
		}
	}

	blank_statusbar();
	wnoutrefresh(bottomwin);
	currmenu = MMAIN;
	display_main_list();
}

/* Our own version of "wc".  Note that its character counts are in
 * multibyte characters instead of single-byte characters. */
void do_wordlinechar_count(void)
{
	size_t words = 0, chars = 0;
	ssize_t nlines = 0;
	size_t current_x_save = openfile->current_x;
	size_t pww_save = openfile->placewewant;
	filestruct *current_save = openfile->current;
	bool old_mark_set = openfile->mark_set;
	filestruct *top, *bot;
	size_t top_x, bot_x;

	if (old_mark_set) {
		/* If the mark is on, partition the filestruct so that it
		 * contains only the marked text, and turn the mark off. */
		mark_order((const filestruct **)&top, &top_x, (const filestruct **)&bot, &bot_x, NULL);
		filepart = partition_filestruct(top, top_x, bot, bot_x);
		openfile->mark_set = FALSE;
	}

	/* Start at the top of the file. */
	openfile->current = openfile->fileage;
	openfile->current_x = 0;
	openfile->placewewant = 0;

	/* Keep moving to the next word (counting punctuation characters as
	 * part of a word, as "wc -w" does), without updating the screen,
	 * until we reach the end of the file, incrementing the total word
	 * count whenever we're on a word just before moving. */
	while (openfile->current != openfile->filebot || openfile->current->data[openfile->current_x] != '\0') {
		if (do_next_word(TRUE, FALSE)) {
			words++;
		}
	}

	/* Get the total line and character counts, as "wc -l"  and "wc -c"
	 * do, but get the latter in multibyte characters. */
	if (old_mark_set) {
		nlines = openfile->filebot->lineno - openfile->fileage->lineno + 1;
		chars = get_totsize(openfile->fileage, openfile->filebot);

		/* Unpartition the filestruct so that it contains all the text
		 * again, and turn the mark back on. */
		unpartition_filestruct(&filepart);
		openfile->mark_set = TRUE;
	} else {
		nlines = openfile->filebot->lineno;
		chars = openfile->totsize;
	}

	/* Restore where we were. */
	openfile->current = current_save;
	openfile->current_x = current_x_save;
	openfile->placewewant = pww_save;

	/* Display the total word, line, and character counts on the statusbar. */
	statusbar(_("%sWords: %lu  Lines: %ld  Chars: %lu"), old_mark_set ? _("In Selection:  ") : "", (unsigned long)words, (long)nlines, (unsigned long)chars);
}

/* Get verbatim input. */
void do_verbatim_input(void)
{
	/* TRANSLATORS: This is displayed when the next keystroke will be
	 * inserted verbatim. */
	statusbar(_("Verbatim Input"));

	/* Read in all the verbatim characters. */
	std::string kbinput = get_verbatim_kbinput(edit);

	/* If constant cursor position display is on, make sure the current
	 * cursor position will be properly displayed on the statusbar.
	 * Otherwise, blank the statusbar. */
	if (ISSET(CONST_UPDATE)) {
		do_cursorpos(TRUE);
	} else {
		blank_statusbar();
		wnoutrefresh(bottomwin);
	}

	/* Display all the verbatim characters at once, not filtering out control characters. */
	do_output(kbinput, TRUE);
}
