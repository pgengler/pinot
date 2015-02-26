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

#include <sstream>
#include <vector>

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
static bool prepend_wrap = false;
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

/* Do deletion of a single character (Delete or Backspace) */
void do_deletion(UndoType action)
{
	size_t orig_lenpt = 0;

	assert(openfile->current != NULL && openfile->current->data != NULL && openfile->current_x <= strlen(openfile->current->data));

	openfile->placewewant = xplustabs();

	if (openfile->current->data[openfile->current_x] != '\0') {
		int char_buf_len = parse_mbchar(openfile->current->data + openfile->current_x, NULL, NULL);
		size_t line_len = strlen(openfile->current->data + openfile->current_x);

		assert(openfile->current_x < strlen(openfile->current->data));

		update_undo(action);

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

		add_undo(action);

		/* If we're deleting at the end of a line, we need to call edit_refresh(). */
		if (openfile->current->data[openfile->current_x] == '\0') {
			edit_refresh_needed = true;
		}

		openfile->current->data = charealloc(openfile->current->data, strlen(openfile->current->data) + strlen(foo->data) + 1);
		strcat(openfile->current->data, foo->data);
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

	if (ISSET(SOFTWRAP) && edit_refresh_needed == false)
		if (strlenpt(openfile->current->data) / COLS != orig_lenpt / COLS) {
			edit_refresh_needed  = true;
		}

	set_modified();

	if (edit_refresh_needed  == false) {
		update_line(openfile->current, openfile->current_x);
	}
}

/* Delete the character under the cursor. */
void do_delete(void)
{
	do_deletion(DEL);
}

/* Backspace over one character.  That is, move the cursor left one
 * character, and then delete the character under the cursor. */
void do_backspace(void)
{
	if (openfile->current != openfile->fileage || openfile->current_x > 0) {
		do_left();
		do_deletion(BACK);
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

		do_output(output, output_len, true);

		free(output);
	} else {
		do_output((char *) "\t", 1, true);
	}
}

/* Indent or unindent the current line (or, if the mark is on, all lines
 * covered by the mark) len columns, depending on whether len is
 * positive or negative.  If the TABS_TO_SPACES flag is set, indent or
 * unindent by len spaces.  Otherwise, indent or unindent by (len /
 * tabsize) tabs and (len % tabsize) spaces. */
void do_indent(ssize_t cols)
{
	bool indent_changed = false;
	/* Whether any indenting or unindenting was done. */
	bool unindent = false;
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

	/* If cols is negative, make it positive and set unindent to true. */
	if (cols < 0) {
		cols = -cols;
		unindent = true;
		/* Otherwise, we're indenting, in which case the file will always be
		 * modified, so set indent_changed to true. */
	} else {
		indent_changed = true;
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

				/* We've unindented, so set indent_changed to true. */
				if (!indent_changed) {
					indent_changed = true;
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
		edit_refresh_needed = true;
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
void redo_paste(undo *u)
{
	/* If we cut the magicline may was well not crash :/ */
	if (!u->cutbuffer) {
		return;
	}

	/* Get to where we need to uncut from */
	if (u->xflags == UNcut_cutline) {
		goto_line_posx(u->mark_begin_lineno, 0);
	} else {
		goto_line_posx(u->mark_begin_lineno, u->mark_begin_x);
	}

	copy_from_filestruct(u->cutbuffer);

	if (u->xflags != UNcut_marked_forward && u->type != PASTE) {
		goto_line_posx(u->mark_begin_lineno, u->mark_begin_x);
	}
}

/* Re-do a cut, or undo an uncut */
void undo_paste(undo *u)
{
	/* If we cut the magicline may was well not crash :/ */
	if (!u->cutbuffer) {
		return;
	}

	filestruct *oldcutbuffer = cutbuffer, *oldcutbottom = cutbottom;
	cutbuffer = cutbottom = NULL;

  goto_line_posx(u->lineno, u->begin);

	if (ISSET(NO_NEWLINES) && openfile->current->lineno != u->lineno) {
		openfile->current_x = strlen(openfile->current->data);
		openfile->placewewant = xplustabs();
	}

	openfile->mark_set = true;
	openfile->mark_begin = fsfromline(u->mark_begin_lineno);
	openfile->mark_begin_x = (u->xflags == UNcut_cutline) ? 0 : u->mark_begin_x;

	do_cut_text(FALSE, FALSE, TRUE);

	openfile->mark_set = false;
	openfile->mark_begin = NULL;
	openfile->mark_begin_x = 0;
	edit_refresh_needed = true;

	if (cutbuffer) {
		free_filestruct(cutbuffer);
	}
	cutbuffer = oldcutbuffer;
	cutbottom = oldcutbottom;
}

/* Undo the last thing(s) we did */
void do_undo(void)
{
	undo *u = openfile->current_undo;
	filestruct *t = nullptr;
	size_t len = 0;
	char *data, *undidmsg = NULL;
	filestruct *oldcutbuffer = cutbuffer, *oldcutbottom = cutbottom;

	if (!u) {
		statusbar(_("Nothing in undo buffer!"));
		return;
	}

	filestruct *f = fsfromline(u->mark_begin_lineno);
	if (!f) {
		statusbar(_("Internal error: can't match line %d.  Please save your work."), u->mark_begin_lineno);
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
		goto_line_posx(u->lineno, u->begin);
		break;
	case BACK:
	case DEL:
		undidmsg = _("text delete");
		len = strlen(f->data) + strlen(u->strdata) + 1;
		data = charalloc(len);

		strncpy(data, f->data, u->begin);
		strcpy(&data[u->begin], u->strdata);
		strcpy(&data[u->begin + strlen(u->strdata)], &f->data[u->begin]);
		free(f->data);
		f->data = data;
		goto_line_posx(u->mark_begin_lineno, u->mark_begin_x);
		break;
	case SPLIT_END:
		goto_line_posx(u->lineno, u->begin);
		openfile->current_undo = openfile->current_undo->next;
		openfile->last_action = OTHER;
		while (openfile->current_undo->type != SPLIT_BEGIN) {
			do_undo();
		}
		u = openfile->current_undo;
		f = openfile->current;
		break;
	case SPLIT_BEGIN:
		undidmsg = _("text add");
		break;
	case JOIN:
		undidmsg = _("line join");
		t = make_new_node(f);
		t->data = mallocstrcpy(NULL, u->strdata);
		data = mallocstrncpy(NULL, f->data, u->mark_begin_x + 1);
		data[u->mark_begin_x] = '\0';
		free(f->data);
		f->data = data;
		splice_node(f, t, f->next);
		if (f == openfile->filebot) {
			openfile->filebot = t;
		}
		goto_line_posx(u->lineno, u->begin);
		break;
	case CUT_EOF:
	case CUT:
		undidmsg = _("text cut");
		redo_paste(u);
		f = fsfromline(u->lineno);
		break;
	case PASTE:
		undidmsg = _("text uncut");
		undo_paste(u);
		f = fsfromline(u->lineno);
		break;
	case ENTER:
		undidmsg = _("line break");
		if (f->next) {
			filestruct *foo = f->next;
			f->data = (char *) nrealloc(f->data, strlen(f->data) + strlen(&f->next->data[u->mark_begin_x]) + 1);
			strcat(f->data, &f->next->data[u->mark_begin_x]);
			if (foo == openfile->filebot) {
				openfile->filebot = f;
			}
			unlink_node(foo);
			delete_node(foo);
		}
		goto_line_posx(u->lineno, u->begin);
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
		openfile->mark_set = true;
		goto_line_posx(u->lineno, u->begin);
		cut_marked();
		if (u->cutbuffer) {
			free_filestruct(u->cutbuffer);
		}
		u->cutbuffer = cutbuffer;
		u->cutbottom = cutbottom;
		cutbuffer = oldcutbuffer;
		cutbottom = oldcutbottom;
		openfile->mark_set = false;
		break;
	case REPLACE:
		undidmsg = _("text replace");
		goto_line_posx(u->lineno, u->begin);
		data = u->strdata;
		u->strdata = f->data;
		f->data = data;
		break;

	default:
		undidmsg = _("Internal error: unknown type.  Please save your work.");
		break;

	}
	if (undidmsg) {
		statusbar(_("Undid action (%s)"), undidmsg);
	}
	renumber(f);
	openfile->current_undo = openfile->current_undo->next;
	openfile->last_action = OTHER;
	openfile->placewewant = xplustabs();
	set_modified();
}

void do_redo(void)
{
	undo *u = openfile->undotop;
	size_t len = 0;
	char *data, *redidmsg = NULL;

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

	filestruct *f = fsfromline(u->mark_begin_lineno);
	if (!f) {
		statusbar(_("Internal error: can't match line %d.  Please save your work."), u->mark_begin_lineno);
		return;
	}
	DEBUG_LOG("data we're about to redo = \"" << f->data << '"');
	DEBUG_LOG("Redo running for type " << u->type);

	switch(u->type) {
	case ADD:
		redidmsg = _("text add");
		len = strlen(f->data) + strlen(u->strdata) + 1;
		data = charalloc(len);
		strncpy(data, f->data, u->begin);
		strcpy(&data[u->begin], u->strdata);
		strcpy(&data[u->begin + strlen(u->strdata)], &f->data[u->begin]);
		free(f->data);
		f->data = data;
		goto_line_posx(u->mark_begin_lineno, u->mark_begin_x);
		break;
	case BACK:
	case DEL:
		redidmsg = _("text delete");
		len = strlen(f->data) + strlen(u->strdata) + 1;
		data = charalloc(len);
		strncpy(data, f->data, u->begin);
		strcpy(&data[u->begin], &f->data[u->begin + strlen(u->strdata)]);
		free(f->data);
		f->data = data;
		openfile->current_x = u->begin;
		goto_line_posx(u->mark_begin_lineno, u->mark_begin_x);
		break;
	case ENTER:
		redidmsg = _("line break");
		goto_line_posx(u->lineno, u->begin);
		do_enter(true);
		break;
	case SPLIT_BEGIN:
		goto_line_posx(u->lineno, u->begin);
		openfile->current_undo = u;
		openfile->last_action = OTHER;
		while (openfile->current_undo->type != SPLIT_END) {
			do_redo();
		}
		u = openfile->current_undo;
		goto_line_posx(u->lineno, u->begin);
		break;
	case SPLIT_END:
		redidmsg = _("text add");
		break;
	case JOIN:
		redidmsg = _("line join");
		len = strlen(f->data) + strlen(u->strdata) + 1;
		f->data = charealloc(f->data, len);
		strcat(f->data, u->strdata);
		if (f->next != NULL) {
			filestruct *tmp = f->next;
			if (tmp == openfile->filebot) {
				openfile->filebot = f;
			}
			unlink_node(tmp);
			delete_node(tmp);
		}
		renumber(f);
		goto_line_posx(u->lineno, u->begin);
		break;
	case CUT_EOF:
	case CUT:
		redidmsg = _("text cut");
		undo_paste(u);
		break;
	case PASTE:
		redidmsg = _("text uncut");
		redo_paste(u);
		break;
	case REPLACE:
		redidmsg = _("text replace");
		data = u->strdata;
		u->strdata = f->data;
		f->data = data;
		goto_line_posx(u->lineno, u->begin);
		break;
	case INSERT:
		redidmsg = _("text insert");
		goto_line_posx(u->lineno, u->begin);
		copy_from_filestruct(u->cutbuffer);
		free_filestruct(u->cutbuffer);
		u->cutbuffer = NULL;
		break;
	default:
		redidmsg = _("Internal error: unknown type.  Please save your work.");
		break;

	}

	if (redidmsg) {
		statusbar(_("Redid action (%s)"), redidmsg);
	}

	openfile->current_undo = u;
	openfile->last_action = OTHER;
	openfile->placewewant = xplustabs();
	set_modified();
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

	if (!undoing) {
		update_undo(ENTER);
	}

	edit_refresh_needed = true;
}

/* Need this again... */
void do_enter_void(void)
{
	do_enter(false);
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

/* Execute command in a shell.  Return true on success. */
bool execute_command(const std::string& command)
{
	int fd[2];
	FILE *f;
	char *shellenv;
	struct sigaction oldaction, newaction;
	/* Original and temporary handlers for SIGINT. */
	bool sig_failed = false;
	/* Did sigaction() fail without changing the signal handlers? */

	/* Make our pipes. */
	if (pipe(fd) == -1) {
		statusbar(_("Could not create pipe"));
		return false;
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
		execl(shellenv, tail(shellenv), "-c", command.c_str(), NULL);
		exit(0);
	}

	/* Continue as parent. */
	close(fd[1]);

	if (pid == -1) {
		close(fd[0]);
		statusbar(_("Could not fork"));
		return false;
	}

	/* Before we start reading the forked command's output, we set
	 * things up so that Ctrl-C will cancel the new process. */

	/* Enable interpretation of the special control keys so that we get
	 * SIGINT when Ctrl-C is pressed. */
	enable_signals();

	if (sigaction(SIGINT, NULL, &newaction) == -1) {
		sig_failed = true;
		nperror("sigaction");
	} else {
		newaction.sa_handler = cancel_command;
		if (sigaction(SIGINT, &newaction, &oldaction) == -1) {
			sig_failed = true;
			nperror("sigaction");
		}
	}

	/* Note that now oldaction is the previous SIGINT signal handler,
	 * to be restored later. */

	f = fdopen(fd[0], "rb");
	if (f == NULL) {
		nperror("fdopen");
	}

	read_file(f, 0, "stdin", true, false);

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

	return true;
}

/* Execute command in a shell without saving its output. Returns -1 if an
 * an error prevented execution, and the command's exit code if it was run. */
int execute_command_silently(const std::string& command)
{
	int fd[2];
	char *shellenv;
	struct sigaction oldaction, newaction;
	/* Original and temporary handlers for SIGINT. */
	bool sig_failed = false;
	/* Did sigaction() fail without changing the signal handlers? */
	int status;

	/* Make our pipes. */
	if (pipe(fd) == -1) {
		statusbar(_("Could not pipe"));
		return false;
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
		execl(shellenv, tail(shellenv), "-c", command.c_str(), NULL);
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
		sig_failed = true;
		nperror("sigaction");
	} else {
		newaction.sa_handler = cancel_command;
		if (sigaction(SIGINT, &newaction, &oldaction) == -1) {
			sig_failed = true;
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
void add_undo(UndoType current_action)
{
	undo *u;
	char *data;
	std::list<OpenFile>::iterator fs = openfile;

	/* Ugh, if we were called while cutting not-to-end, non-marked and on the same lineno,
	   we need to  abort here */
	u = fs->current_undo;
	if (u && u->mark_begin_lineno == fs->current->lineno && ((current_action == CUT && u->type == CUT && !u->mark_set && keeping_cutbuffer()) || (current_action == ADD && u->type == ADD && u->mark_begin_x == fs->current_x))) {
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
	if (u->type == SPLIT_BEGIN) {
		/* Some action, most likely an ADD, was performed that invoked do_wrap().  Rearrange the undo order so that this previous action is after the SPLIT_BEGIN undo. */
		u->next = fs->undotop->next;
		fs->undotop->next = u;
	} else {
		u->next = fs->undotop;
		fs->undotop = u;
		fs->current_undo = u;
	}
	u->strdata = NULL;
	u->cutbuffer = NULL;
	u->cutbottom  = NULL;
	u->mark_set = 0;
	u->mark_begin_lineno = fs->current->lineno;
	u->mark_begin_x = fs->current_x;
	u->xflags = 0;

	switch (u->type) {
		/* We need to start copying data into the undo buffer or we wont be able
		   to restore it later */
	case ADD:
		break;
	case BACK:
	case DEL:
		if (u->begin != strlen(fs->current->data)) {
			char *char_buf = charalloc(mb_cur_max() + 1);
			int char_buf_len = parse_mbchar(&fs->current->data[u->begin], char_buf, NULL);
			char_buf[char_buf_len] = '\0';
			u->strdata = char_buf;  /* Note: there is likely more memory allocated than necessary. */
			if (u->type == BACK) {
				u->mark_begin_x += char_buf_len;
			}
			break;
		}
		/* Else purposely fall into join code */
	case JOIN:
		if (fs->current->next) {
			if (u->type == BACK) {
				u->lineno = fs->current->next->lineno;
				u->begin = 0;
			}
			data = mallocstrcpy(NULL, fs->current->next->data);
			u->strdata = data;
		}
		current_action = u->type = JOIN;
		break;
	case SPLIT_BEGIN:
		current_action = fs->undotop->type;
		break;
	case SPLIT_END:
		break;
	case INSERT:
		break;
	case REPLACE:
		data = mallocstrcpy(NULL, fs->current->data);
		u->strdata = data;
		break;
	case CUT_EOF:
		cutbuffer_reset();
		break;
	case CUT:
		cutbuffer_reset();
		u->mark_set = openfile->mark_set;
		if (u->mark_set) {
			u->mark_begin_lineno = openfile->mark_begin->lineno;
			u->mark_begin_x = openfile->mark_begin_x;
		} else if (!ISSET(CUT_TO_END)) {
			/* The entire line is being cut regardless of the cursor position. */
			u->begin = 0;
			u->xflags = UNcut_cutline;
		}
		break;
	case PASTE:
		if (!cutbuffer) {
			statusbar(_("Internal error: can't setup uncut.  Please save your work."));
		} else {
			if (u->cutbuffer) {
				free_filestruct(u->cutbuffer);
			}
			u->cutbuffer = copy_filestruct(cutbuffer);
			u->mark_begin_lineno = fs->current->lineno;
			u->mark_begin_x =  fs->current_x;
			u->lineno =  fs->current->lineno + cutbottom->lineno - cutbuffer->lineno;
			u->mark_set = true;
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
void update_undo(UndoType action)
{
	undo *u;
	std::list<OpenFile>::iterator fs = openfile;

	DEBUG_LOG("action = " << action << ", fs->last_action = " << fs->last_action << ",  openfile->current->lineno = " << openfile->current->lineno);
	if (fs->current_undo) {
		DEBUG_LOG("fs->current_undo->lineno = " << fs->current_undo->lineno);
	}

	/* Change to an add if we're not using the same undo struct
	   that we should be using */
	if (action != fs->last_action || (action != ENTER && action != CUT && action != INSERT && openfile->current->lineno != fs->current_undo->lineno)) {
		add_undo(action);
		return;
	}

	assert(fs->undotop != NULL);
	u = fs->undotop;

	switch (u->type) {
	case ADD:
		DEBUG_LOG("fs->current->data = \"" << fs->current->data << "\", current_x = " << fs->current_x << ", u->begin = " << u->begin);
		{
			char *char_buf = charalloc(mb_cur_max());
			size_t char_buf_len = parse_mbchar(&fs->current->data[u->mark_begin_x], char_buf, NULL);
			u->strdata = addstrings(u->strdata, u->strdata ? strlen(u->strdata) : 0, char_buf, char_buf_len);
		}
		DEBUG_LOG("current undo data now \"" << u->strdata << '"');
		u->mark_begin_lineno = fs->current->lineno;
		u->mark_begin_x = fs->current_x;
		break;
	case BACK:
	case DEL:
		{
			char *char_buf = charalloc(mb_cur_max());
			size_t char_buf_len = parse_mbchar(&fs->current->data[fs->current_x], char_buf, NULL);
			if (fs->current_x == u->begin) {
				/* They're deleting */
				u->strdata = addstrings(u->strdata, strlen(u->strdata), char_buf, char_buf_len);
				u->mark_begin_x = fs->current_x;
			} else if (fs->current_x == u->begin - char_buf_len) {
				/* They're backspacing */
				u->strdata = addstrings(char_buf, char_buf_len, u->strdata, strlen(u->strdata));
				u->begin = fs->current_x;
			} else {
				/* They deleted something else on the line */
				free(char_buf);
				add_undo(u->type);
				return;
			}
		}
		DEBUG_LOG("current undo data now \"" << u->strdata << '"' << std::endl << "u->begin = " << u->begin);
		break;
	case CUT_EOF:
	case CUT:
		if (!cutbuffer) {
			break;
		}
		if (u->cutbuffer) {
			free_filestruct(u->cutbuffer);
		}
		u->cutbuffer = copy_filestruct(cutbuffer);
		if (u->mark_set) {
			/* If the "marking" operation was from right-->left or
			 * bottom-->top, then swap the mark points. */
			if ((u->lineno == u->mark_begin_lineno && u->begin < u->mark_begin_x) || u->lineno < u->mark_begin_lineno) {
				size_t x_loc = u->begin;
				u->begin = u->mark_begin_x;
				u->mark_begin_x = x_loc;

				ssize_t line = u->lineno;
				u->lineno = u->mark_begin_lineno;
				u->mark_begin_lineno = line;
			} else {
				u->xflags = UNcut_marked_forward;
			}
		} else {
			/* Compute cutbottom for the uncut using out copy */
			u->cutbottom = u->cutbuffer;
			while (u->cutbottom->next != NULL) {
				u->cutbottom = u->cutbottom->next;
			}
			u->lineno = u->mark_begin_lineno + u->cutbottom->lineno - u->cutbuffer->lineno;
			if (ISSET(CUT_TO_END) || u->type == CUT_EOF) {
				u->begin = strlen(u->cutbottom->data);
				if (u->lineno == u->mark_begin_lineno) {
					u->begin += u->mark_begin_x;
				}
			}
		}
		break;
	case REPLACE:
	case PASTE:
		u->begin = fs->current_x;
		u->lineno = openfile->current->lineno;
		break;
	case INSERT:
		u->mark_begin_lineno = openfile->current->lineno;
		break;
	case ENTER:
		u->mark_begin_x = fs->current_x;
		break;
	case SPLIT_BEGIN:
	case SPLIT_END:
	case JOIN:
		/* These cases are handled by the earlier check for a new line and action */
	case OTHER:
		break;
	}

	DEBUG_LOG("Done in udpate_undo (type was " << action << ')');
}

/* Unset the prepend_wrap flag.  We need to do this as soon as we do
 * something other than type text. */
void wrap_reset(void)
{
	prepend_wrap = false;
}

/* Try wrapping the given line. Return true if wrapped, false otherwise. */
bool do_wrap(filestruct *line)
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
	const char *next_line = NULL;
	/* The next line, minus indentation. */
	size_t next_line_len = 0;
	/* The length of next_line. */

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
	wrap_loc = break_line(line->data, fill, false);

	/* If we couldn't break the line, or we've reached the end of it, we
	 * don't wrap. */
	if (wrap_loc == -1 || line->data[wrap_loc] == '\0') {
		return false;
	}

	/* Otherwise, move forward to the character just after the blank. */
	wrap_loc += move_mbright(line->data + wrap_loc, 0);

	/* If we've reached the end of the line, we don't wrap. */
	if (line->data[wrap_loc] == '\0') {
		return false;
	}

	/* If autoindent is turned on, and we're on the character just after
	 * the indentation, we don't wrap. */
	if (ISSET(AUTOINDENT)) {
		/* Get the indentation of this line. */
		indent_string = line->data;
		indent_len = indent_length(indent_string);

		if (wrap_loc == indent_len) {
			return false;
		}
	}

	add_undo(SPLIT_BEGIN);
	size_t old_x = openfile->current_x;
	filestruct *old_line = openfile->current;
	openfile->current = line;

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

		/* Go to the end of the line. */
		openfile->current_x = line_len;

		/* If after_break doesn't end in a blank, make sure it ends in a
		 * space. */
		if (!is_blank_mbchar(end)) {
			add_undo(ADD);
			line_len++;
			line->data = charealloc(line->data, line_len + 1);
			line->data[line_len - 1] = ' ';
			line->data[line_len] = '\0';
			after_break = line->data + wrap_loc;
			after_break_len++;
			openfile->totsize++;
			openfile->current_x++;
			update_undo(ADD);
		}

		next_line = line->next->data;
		next_line_len = strlen(next_line);

		if (after_break_len + next_line_len <= fill) {
			/* Delete the LF to join the two lines. */
			do_delete();
			/* Delete any leading blanks from the joined-on line. */
			while (is_blank_mbchar(&line->data[openfile->current_x])) {
				do_delete();
			}
			renumber(line);
		}
	}

	/* Go to the wrap location and split the line there. */
	openfile->current_x = wrap_loc;
	do_enter(false);

	if (old_x < wrap_loc) {
		openfile->current_x = old_x;
		openfile->current = old_line;
		prepend_wrap = true;
	} else {
		openfile->current_x += (old_x - wrap_loc);
		prepend_wrap = false;
	}
	openfile->placewewant = xplustabs();

	add_undo(SPLIT_END);

	return true;
}

/* We are trying to break a chunk off line.  We find the last blank such
 * that the display length to there is at most (goal + 1).  If there is
 * no such blank, then we find the first blank.  We then take the last
 * blank in that group of blanks.  The terminating '\0' counts as a
 * blank, as does a '\n' if newline is true. */
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
		bool found_blank = false;
		ssize_t found_blank_loc = 0;

		while (*line != '\0') {
			line_len = parse_mbchar(line, NULL, NULL);

			if (is_blank_mbchar(line) || (newln && *line == '\n')) {
				if (!found_blank) {
					found_blank = true;
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

	while (*line != '\0' && is_blank_mbchar(line)) {
		line_len = parse_mbchar(line, NULL, NULL);

		line += line_len;
		blank_loc += line_len;
	}

	return blank_loc;
}

/* The "indentation" of a line is the whitespace between the quote part
 * and the non-whitespace of the line. */
size_t indent_length(const std::string& line)
{
	size_t len = 0;
	char *blank_mb;
	int blank_mb_len;

	blank_mb = charalloc(mb_cur_max());

	size_t i = 0;
	while (i < line.length()) {
		blank_mb_len = parse_mbchar(line.c_str() + i, blank_mb, NULL);

		if (!is_blank_mbchar(blank_mb)) {
			break;
		}

		i += blank_mb_len;
		len += blank_mb_len;
	}

	free(blank_mb);

	return len;
}

/* A word is misspelled in the file.  Let the user replace it.  We
 * return false if the user cancels. */
bool do_int_spell_fix(const char *word)
{
	size_t match_len, current_x_save = openfile->current_x;
	size_t pww_save = openfile->placewewant;
	filestruct *edittop_save = openfile->edittop;
	filestruct *current_save = openfile->current;
	/* Save where we are. */
	bool canceled = false;
	/* The return value. */
	bool case_sens_set = ISSET(CASE_SENSITIVE);
	bool backwards_search_set = ISSET(BACKWARDS_SEARCH);
	bool regexp_set = ISSET(USE_REGEXP);
	bool old_mark_set = openfile->mark_set;
	bool added_magicline = false;
	/* Whether we added a magicline after filebot. */
	bool right_side_up = false;
	/* true if (mark_begin, mark_begin_x) is the top of the mark,
	 * false if (current, current_x) is. */
	filestruct *top, *bot;
	size_t top_x, bot_x;

	/* Make sure spell-check is case sensitive. */
	SET(CASE_SENSITIVE);

	/* Make sure spell-check goes forward only. */
	UNSET(BACKWARDS_SEARCH);
	/* Make sure spell-check doesn't use regular expressions. */
	UNSET(USE_REGEXP);

	/* Save the current search/replace strings. */
	auto save_search = last_search;
	auto save_replace = last_replace;

	/* Set the search/replace strings to the misspelled word. */
	last_search = word;
	last_replace = word;

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
		openfile->mark_set = false;
	}

	/* Start from the top of the file. */
	openfile->edittop = openfile->fileage;
	openfile->current = openfile->fileage;
	openfile->current_x = (size_t)-1;
	openfile->placewewant = 0;

	/* Find the first whole occurrence of word. */
	findnextstr_wrap_reset();
	while (findnextstr(true, false, openfile->fileage, 0, word, &match_len)) {
		if (is_whole_word(openfile->current_x, openfile->current->data, word)) {
			size_t xpt = xplustabs();
			char *exp_word = display_string(openfile->current->data, xpt, strnlenpt(openfile->current->data, openfile->current_x + match_len) - xpt, false);

			edit_refresh();

			do_replace_highlight(true, exp_word);

			/* Allow all instances of the word to be corrected. */
			std::shared_ptr<Key> key;
			canceled = (do_prompt(false, true, MSPELL, key, word, NULL, edit_refresh, _("Edit a replacement")) == PROMPT_ABORTED);

			do_replace_highlight(false, exp_word);

			free(exp_word);

			if (!canceled && word != answer) {
				openfile->current_x--;
				do_replace_loop(true, &canceled, openfile->current, &openfile->current_x, word);
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
		openfile->mark_set = true;
	}

	/* Restore the search/replace strings. */
	last_search = save_search;
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
	edit_refresh_needed = true;

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
	bool added_magicline = false;
	/* Whether we added a magicline after filebot. */
	bool right_side_up = false;
	/* true if (mark_begin, mark_begin_x) is the top of the mark,
	 * false if (current, current_x) is. */
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
		openfile->mark_set = false;
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
	allow_pending_sigwinch(false);

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

		/* Turn the mark back on if it was on before. */
		openfile->mark_set = old_mark_set;

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
		openfile->mark_set = true;
	}

	/* Go back to the old position, and mark the file as modified. */
	do_gotopos(lineno_save, current_x_save, current_y_save, pww_save);
	set_modified();

	/* Handle a pending SIGWINCH again. */
	allow_pending_sigwinch(true);

	return NULL;
}

/* Spell check the current file.  If an alternate spell checker is
 * specified, use it.  Otherwise, use the internal spell checker. */
void do_spell(void)
{
	bool status;
	FILE *temp_file;
	char *temp = mallocstrcpy(NULL, safe_tempfile(&temp_file).c_str());
	const char *spell_msg;

	if (temp == NULL) {
		statusbar(_("Error writing temp file: %s"), strerror(errno));
		return;
	}

	status = openfile->mark_set ? write_marked_file(temp, temp_file, true, OVERWRITE) : write_file(temp, temp_file, true, OVERWRITE, false);

	if (!status) {
		statusbar(_("Error writing temp file: %s"), strerror(errno));
		free(temp);
		return;
	}

	blank_bottombars();
	statusbar(_("Invoking spell checker, please wait..."));
	doupdate();

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

 /* Cleanup things to do after leaving the linter */
void lint_cleanup(void)
{
	currmenu = MMAIN;
	display_main_list();
}

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
	LintMessages lints;

	if (!openfile->syntax || openfile->syntax->linter == "") {
		statusbar(_("No linter defined for this file!"));
		return;
	}

	if (openfile->modified) {
		int i = do_yesno_prompt(false, _("Save modified buffer before linting?"));

		if (i == -1) {
			statusbar(_("Cancelled"));
			lint_cleanup();
			return;
		} else if (i == 1) {
			if (do_writeout(false) != true) {
				lint_cleanup();
				return;
			}
		}
	}

	lintcopy = mallocstrcpy(NULL, openfile->syntax->linter.c_str());
	/* Create pipe up front. */
	if (pipe(lint_fd) == -1) {
		statusbar(_("Could not create pipe"));
		lint_cleanup();
		return;
	}

	blank_bottombars();
	statusbar(_("Invoking linter, please wait..."));
	doupdate();

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
	lintargs[arglen - 2] = mallocstrcpy(NULL, openfile->filename.c_str());

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

		free(lintargs[arglen - 2]);
		free(lintargs);

		/* This should not be reached if linter is found. */
		exit(1);
	}

	/* Parent continues here. */
	close(lint_fd[1]);

	/* The child process was not forked successfully. */
	if (pid_lint < 0) {
		close(lint_fd[0]);
		statusbar(_("Could not fork"));
		lint_cleanup();
		return;
	}

	/* Get the system pipe buffer size. */
	if ((pipe_buff_size = fpathconf(lint_fd[0], _PC_PIPE_BUF)) < 1) {
		close(lint_fd[0]);
		statusbar(_("Could not get size of pipe buffer"));
		lint_cleanup();
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
		lint_cleanup();
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
			if (stat(curr_lint->filename, &lintfileinfo) != -1) {
				if (openfile->current_stat->st_ino != lintfileinfo.st_ino) {
					std::list<OpenFile>::iterator tmpof = openfile;
					while (tmpof != std::next(openfile, 1)) {
						if (tmpof->current_stat->st_ino == lintfileinfo.st_ino) {
							break;
						}
						++tmpof;
					}
					if (tmpof->current_stat->st_ino != lintfileinfo.st_ino) {
						char *msg = charalloc(1024 + curr_lint->filename.length());
						int i;

						sprintf(msg, _("This message is for unopened file %s, open it in a new buffer?"), curr_lint->filename.c_str());
						i = do_yesno_prompt(false, msg);
						free(msg);
						if (i == -1) {
							statusbar(_("Cancelled"));
							lint_cleanup();
							return;
						} else if (i == 1) {
							SET(MULTIBUFFER);
							open_buffer(curr_lint->filename, false);
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
			do_gotolinecolumn(curr_lint->lineno, tmpcol, false, false, false, false);
			titlebar(NULL);
			edit_refresh();
			statusbar(curr_lint->text.c_str());
			bottombars(MLINTER);
		}

		Key kbinput = get_kbinput(bottomwin);
		auto func = func_from_key(kbinput);
		last_lint = curr_lint;

		if (func == do_cancel) {
			break;
		} else if (func == do_help_void) {
			do_help_void();
		} else if (func == do_page_down) {
			if (curr_lint == lints.end()) {
				statusbar(_("At last message"));
			} else {
				++curr_lint;
			}
		} else if (func == do_page_up) {
			if (curr_lint == lints.begin()) {
				statusbar(_("At first message"));
			} else {
				--curr_lint;
			}
		}
	}

	lint_cleanup();
}

/* Run a formatter for the given syntax.
 * Expects the formatter to be non-interactive and
 * operate on a file in-place, which we'll pass it
 * on the command line.  Another mashuhp of the speller
 * and alt_speller routines.
 */
void do_formatter(void)
{
	bool status;
	FILE *temp_file;
	std::string temp = safe_tempfile(&temp_file);
	int format_status;
	size_t current_x_save = openfile->current_x;
	size_t pww_save = openfile->placewewant;
	ssize_t current_y_save = openfile->current_y;
	ssize_t lineno_save = openfile->current->lineno;
	pid_t pid_format;
	char *finalstatus = NULL;

	/* Check whether we're using syntax highlighting and formatter option is set */
	if (!openfile->syntax || openfile->syntax->formatter == "") {
		statusbar(_("Error: no formatter defined"));
		return;
	}

	if (temp == "") {
		statusbar(_("Error creating temp file: %s"), strerror(errno));
		return;
	}

	/* we're not supporting partial formatting, oi vey */
	openfile->mark_set = FALSE;
	status = write_file(temp, temp_file, TRUE, OVERWRITE, FALSE);

	if (!status) {
		statusbar(_("Error writing temp file: %s"), strerror(errno));
		return;
	}

	if (openfile->totsize == 0) {
		statusbar(_("Finished"));
		return;
	}

	blank_bottombars();
	statusbar(_("Invoking formatter, please wait"));
	doupdate();

	endwin();

	/* Set up an argument list to pass execvp(). */
	std::vector<std::string> format_args;
	std::stringstream stream(openfile->syntax->formatter);
	std::string arg;
	while (stream >> arg) {
		format_args.push_back(arg);
	}
	format_args.push_back(temp);
	char **argbuf;

	/* Start a new process for the formatter. */
	if ((pid_format = fork()) == 0) {
		/* Start alternate format program; we are using $PATH. */
		execvp(format_args[0], format_args, &argbuf);

		/* Should not be reached, if alternate formatter is found!!! */
		exit(1);
	}

	/* If we couldn't fork, get out. */
	if (pid_format < 0) {
		statusbar(_("Could not fork"));
		return;
	}

	/* Don't handle a pending SIGWINCH until the alternate format checker
	 * is finished and we've loaded the format-checked file back in. */
	allow_pending_sigwinch(FALSE);

	/* Wait for the formatter to finish. */
	wait(&format_status);
	free(argbuf);

	/* Reenter curses mode. */
	doupdate();

	/* Restore the terminal to its previous state. */
	terminal_init();

	/* Turn the cursor back on for sure. */
	curs_set(1);

	/* The screen might have been resized.  If it has, reinitialize all
	 * the windows based on the new screen dimensions. */
	window_init();

	if (!WIFEXITED(format_status) || WEXITSTATUS(format_status) != 0) {
		char *format_error;
		char *invoke_error = _("Error invoking \"%s\"");

		format_error = charalloc(strlen(invoke_error) + openfile->syntax->formatter.length() + 1);
		sprintf(format_error, invoke_error, openfile->syntax->formatter.c_str());
		finalstatus = format_error;
	} else {
		/* Replace the text of the current buffer with the format-checked text. */
		replace_buffer(temp);

		/* Go back to the old position, and mark the file as modified. */
		do_gotopos(lineno_save, current_x_save, current_y_save, pww_save);
		set_modified();

		/* Handle a pending SIGWINCH again. */
		allow_pending_sigwinch(TRUE);

		finalstatus = _("Finished formatting");
	}

	unlink(temp);

	currmenu = MMAIN;

	/* If the spell-checker printed any error messages onscreen, make
	 * sure that they're cleared off. */
	total_refresh();

	statusbar(finalstatus);
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
		openfile->mark_set = false;
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
		if (do_next_word(true, false)) {
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
		openfile->mark_set = true;
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
		do_cursorpos(true);
	} else {
		blank_statusbar();
		wnoutrefresh(bottomwin);
	}

	/* Display all the verbatim characters at once, not filtering out control characters. */
	do_output(kbinput, true);
}
