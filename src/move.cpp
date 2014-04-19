/**************************************************************************
 *   move.c                                                               *
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

#include <string.h>
#include <ctype.h>

/* Move to the first line of the file. */
void do_first_line(void)
{
	openfile->current = openfile->edittop = openfile->fileage;
	openfile->current_x = 0;
	openfile->placewewant = 0;

	edit_refresh_needed = 1;
}

/* Move to the last line of the file. */
void do_last_line(void)
{
	openfile->current = openfile->filebot;
	openfile->current_x = strlen(openfile->filebot->data);
	openfile->placewewant = xplustabs();
	openfile->current_y = editwinrows - 1;

	edit_refresh_needed = 1;
}

/* Move up one page. */
void do_page_up(void)
{
	int i, skipped = 0;

	/* If there's less than a page of text left on the screen, put the
	 * cursor at the beginning of the first line of the file, and then
	 * update the edit window. */
	if (openfile->current->lineno == 1 || (!ISSET(SOFTWRAP) && openfile->current->lineno <= editwinrows - 2)) {
		do_first_line();
		return;
	}

	/* If we're not in smooth scrolling mode, put the cursor at the
	 * beginning of the top line of the edit window, as Pico does. */

	if (!ISSET(SMOOTH_SCROLL)) {
		openfile->current = openfile->edittop;
		openfile->placewewant = openfile->current_y = 0;
	}

	for (i = editwinrows - 2; i - skipped > 0 && openfile->current != openfile->fileage; i--) {
		openfile->current = openfile->current->prev;
		if (ISSET(SOFTWRAP) && openfile->current) {
			skipped += strlenpt(openfile->current->data) / COLS;
			DEBUG_LOG("do_page_up: i = " << i << ", skipped = " << skipped << " based on line " << openfile->current->lineno << "  len " << strlenpt(openfile->current->data));
		}
	}

	openfile->current_x = actual_x(openfile->current->data, openfile->placewewant);

	DEBUG_LOG("do_page_up: openfile->current->lineno = " << openfile->current->lineno << ", skipped = " << skipped);

	/* Scroll the edit window up a page. */
	edit_update(NONE);
}

/* Move down one page. */
void do_page_down(void)
{
	int i;

	/* If there's less than a page of text left on the screen, put the
	 * cursor at the beginning of the last line of the file, and then
	 * update the edit window. */
	if (openfile->current->lineno + maxrows - 2 >= openfile->filebot->lineno) {
		do_last_line();
		return;
	}

	/* If we're not in smooth scrolling mode, put the cursor at the
	 * beginning of the top line of the edit window, as Pico does. */
	if (!ISSET(SMOOTH_SCROLL)) {
		openfile->current = openfile->edittop;
		openfile->placewewant = openfile->current_y = 0;
	}

	for (i = maxrows - 2; i > 0 && openfile->current != openfile->filebot; i--) {
		openfile->current = openfile->current->next;
		DEBUG_LOG("do_page_down: moving to line " << openfile->current->lineno);
	}

	openfile->current_x = actual_x(openfile->current->data, openfile->placewewant);

	/* Scroll the edit window down a page. */
	edit_update(NONE);
}

/* Move to the next word in the file.  If allow_punct is true, treat
 * punctuation as part of a word.  If allow_update is true, update the
 * screen afterwards.  Return true if we started on a word, and false
 * otherwise. */
bool do_next_word(bool allow_punct, bool allow_update)
{
	size_t pww_save = openfile->placewewant;
	filestruct *current_save = openfile->current;
	char *char_mb;
	int char_mb_len;
	bool end_line = false, started_on_word = false;

	assert(openfile->current != NULL && openfile->current->data != NULL);

	char_mb = charalloc(mb_cur_max());

	/* Move forward until we find the character after the last letter of
	 * the current word. */
	while (!end_line) {
		char_mb_len = parse_mbchar(openfile->current->data + openfile->current_x, char_mb, NULL);

		/* If we've found it, stop moving forward through the current line. */
		if (!is_word_mbchar(char_mb, allow_punct)) {
			break;
		}

		/* If we haven't found it, then we've started on a word, so set
		 * started_on_word to true. */
		started_on_word = true;

		if (openfile->current->data[openfile->current_x] == '\0') {
			end_line = true;
		} else {
			openfile->current_x += char_mb_len;
		}
	}

	/* Move forward until we find the first letter of the next word. */
	if (openfile->current->data[openfile->current_x] == '\0') {
		end_line = true;
	} else {
		openfile->current_x += char_mb_len;
	}

	for (; openfile->current != NULL;
	        openfile->current = openfile->current->next) {
		while (!end_line) {
			char_mb_len = parse_mbchar(openfile->current->data + openfile->current_x, char_mb, NULL);

			/* If we've found it, stop moving forward through the
			 * current line. */
			if (is_word_mbchar(char_mb, allow_punct)) {
				break;
			}

			if (openfile->current->data[openfile->current_x] == '\0') {
				end_line = true;
			} else {
				openfile->current_x += char_mb_len;
			}
		}

		/* If we've found it, stop moving forward to the beginnings of
		 * subsequent lines. */
		if (!end_line) {
			break;
		}

		if (openfile->current != openfile->filebot) {
			end_line = false;
			openfile->current_x = 0;
		}
	}

	free(char_mb);

	/* If we haven't found it, move to the end of the file. */
	if (openfile->current == NULL) {
		openfile->current = openfile->filebot;
	}

	openfile->placewewant = xplustabs();

	/* If allow_update is true, update the screen. */
	if (allow_update) {
		edit_redraw(current_save, pww_save);
	}

	/* Return whether we started on a word. */
	return started_on_word;
}

/* Move to the next word in the file, treating punctuation as part of a
 * word if the WORD_BOUNDS flag is set, and update the screen
 * afterwards. */
void do_next_word_void(void)
{
	do_next_word(ISSET(WORD_BOUNDS), true);
}

/* Move to the previous word in the file.  If allow_punct is true, treat
 * punctuation as part of a word.  If allow_update is true, update the
 * screen afterwards.  Return true if we started on a word, and false
 * otherwise. */
bool do_prev_word(bool allow_punct, bool allow_update)
{
	size_t pww_save = openfile->placewewant;
	filestruct *current_save = openfile->current;
	char *char_mb;
	int char_mb_len;
	bool begin_line = false, started_on_word = false;

	assert(openfile->current != NULL && openfile->current->data != NULL);

	char_mb = charalloc(mb_cur_max());

	/* Move backward until we find the character before the first letter
	 * of the current word. */
	while (!begin_line) {
		char_mb_len = parse_mbchar(openfile->current->data + openfile->current_x, char_mb, NULL);

		/* If we've found it, stop moving backward through the current
		 * line. */
		if (!is_word_mbchar(char_mb, allow_punct)) {
			break;
		}

		/* If we haven't found it, then we've started on a word, so set
		 * started_on_word to true. */
		started_on_word = true;

		if (openfile->current_x == 0) {
			begin_line = true;
		} else
			openfile->current_x = move_mbleft(openfile->current->data, openfile->current_x);
	}

	/* Move backward until we find the last letter of the previous
	 * word. */
	if (openfile->current_x == 0) {
		begin_line = true;
	} else
		openfile->current_x = move_mbleft(openfile->current->data, openfile->current_x);

	for (; openfile->current != NULL; openfile->current = openfile->current->prev) {
		while (!begin_line) {
			char_mb_len = parse_mbchar(openfile->current->data + openfile->current_x, char_mb, NULL);

			/* If we've found it, stop moving backward through the current line. */
			if (is_word_mbchar(char_mb, allow_punct)) {
				break;
			}

			if (openfile->current_x == 0) {
				begin_line = true;
			} else
				openfile->current_x = move_mbleft(openfile->current->data, openfile->current_x);
		}

		/* If we've found it, stop moving backward to the ends of previous lines. */
		if (!begin_line) {
			break;
		}

		if (openfile->current != openfile->fileage) {
			begin_line = false;
			openfile->current_x = strlen(openfile->current->prev->data);
		}
	}

	/* If we haven't found it, move to the beginning of the file. */
	if (openfile->current == NULL) {
		openfile->current = openfile->fileage;
	}
	/* If we've found it, move backward until we find the character
	 * before the first letter of the previous word. */
	else if (!begin_line) {
		if (openfile->current_x == 0) {
			begin_line = true;
		} else {
			openfile->current_x = move_mbleft(openfile->current->data, openfile->current_x);
		}

		while (!begin_line) {
			char_mb_len = parse_mbchar(openfile->current->data + openfile->current_x, char_mb, NULL);

			/* If we've found it, stop moving backward through the current line. */
			if (!is_word_mbchar(char_mb, allow_punct)) {
				break;
			}

			if (openfile->current_x == 0) {
				begin_line = true;
			} else {
				openfile->current_x = move_mbleft(openfile->current->data, openfile->current_x);
			}
		}

		/* If we've found it, move forward to the first letter of the
		 * previous word. */
		if (!begin_line) {
			openfile->current_x += char_mb_len;
		}
	}

	free(char_mb);

	openfile->placewewant = xplustabs();

	/* If allow_update is true, update the screen. */
	if (allow_update) {
		edit_redraw(current_save, pww_save);
	}

	/* Return whether we started on a word. */
	return started_on_word;
}

/* Move to the previous word in the file, treating punctuation as part
 * of a word if the WORD_BOUNDS flag is set, and update the screen
 * afterwards. */
void do_prev_word_void(void)
{
	do_prev_word(ISSET(WORD_BOUNDS), true);
}

/* Move to the beginning of the current line.  If the SMART_HOME flag is
 * set, move to the first non-whitespace character of the current line
 * if we aren't already there, or to the beginning of the current line
 * if we are. */
void do_home(void)
{
	size_t pww_save = openfile->placewewant;

	if (ISSET(SMART_HOME)) {
		size_t current_x_save = openfile->current_x;

		openfile->current_x = indent_length(openfile->current->data);

		if (openfile->current_x == current_x_save || openfile->current->data[openfile->current_x] == '\0') {
			openfile->current_x = 0;
		}

		openfile->placewewant = xplustabs();
	} else {
		openfile->current_x = 0;
		openfile->placewewant = 0;
	}

	if (need_horizontal_update(pww_save)) {
		update_line(openfile->current, openfile->current_x);
	}
}

/* Move to the end of the current line. */
void do_end(void)
{
	size_t pww_save = openfile->placewewant;

	openfile->current_x = strlen(openfile->current->data);
	openfile->placewewant = xplustabs();

	if (need_horizontal_update(pww_save)) {
		update_line(openfile->current, openfile->current_x);
	}
}

/* If scroll_only is false, move up one line.  If scroll_only is true,
 * scroll up one line without scrolling the cursor. */
void do_up(bool scroll_only)
{
	/* If we're at the top of the file, or if scroll_only is true and
	 * the top of the file is onscreen, get out. */
	if (openfile->current == openfile->fileage || (scroll_only && openfile->edittop == openfile->fileage)) {
		return;
	}

	assert(ISSET(SOFTWRAP) || openfile->current_y == openfile->current->lineno - openfile->edittop->lineno);

	/* Move the current line of the edit window up. */
	openfile->current = openfile->current->prev;
	openfile->current_x = actual_x(openfile->current->data, openfile->placewewant);

	/* If scroll_only is false and if we're on the first line of the
	 * edit window, scroll the edit window up one line if we're in
	 * smooth scrolling mode, or up half a page if we're not.  If
	 * scroll_only is true, scroll the edit window up one line
	 * unconditionally. */
	if (openfile->current_y == 0 || (ISSET(SOFTWRAP) && openfile->edittop->lineno == openfile->current->next->lineno) || scroll_only) {
		edit_scroll(UP_DIR, (ISSET(SMOOTH_SCROLL) || scroll_only) ? 1 : editwinrows / 2 + 1);
	}

	/* If we're below the first line of the edit window, update the
	 * line we were on before and the line we're on now.  The former
	 * needs to be redrawn if we're not on the first page, and the
	 * latter needs to be drawn unconditionally. */
	if (openfile->current_y > 0) {
		if (need_vertical_update(0)) {
			update_line(openfile->current->next, 0);
		}
		update_line(openfile->current, openfile->current_x);
	}
}

/* Move up one line. */
void do_up_void(void)
{
	do_up(false);
}

/* Scroll up one line without scrolling the cursor. */
void do_scroll_up(void)
{
	do_up(true);
}

/* If scroll_only is false, move down one line.  If scroll_only is true,
 * scroll down one line without scrolling the cursor. */
void do_down(bool scroll_only)
{
	int amount = 0;
	filestruct *topline;

	/* If we're at the bottom of the file, get out. */
	if (openfile->current == openfile->filebot) {
		return;
	}


	assert(ISSET(SOFTWRAP) || openfile->current_y == openfile->current->lineno - openfile->edittop->lineno);

	/* Move the current line of the edit window down. */
	openfile->current = openfile->current->next;
	openfile->current_x = actual_x(openfile->current->data, openfile->placewewant);

	if (ISSET(SOFTWRAP)) {
		/* Compute the amount to scroll. */
		amount = (strlenpt(openfile->current->data) / COLS + openfile->current_y + 2 + strlenpt(openfile->current->prev->data) / COLS - editwinrows);
		topline = openfile->edittop;
		/* Reduce the amount when there are overlong lines at the top. */
		for (int enough = 1; enough < amount; enough++) {
			if (amount <= strlenpt(topline->data) / COLS) {
				amount = enough;
				break;
			}
			amount -= strlenpt(topline->data) / COLS;
			topline = topline->next;
		}
	}

	/* If scroll_only is false and if we're on the last line of the
	 * edit window, scroll the edit window down one line if we're in
	 * smooth scrolling mode, or down half a page if we're not.  If
	 * scroll_only is true, scroll the edit window down one line
	 * unconditionally. */
	if (openfile->current_y == editwinrows - 1 || amount > 0 || scroll_only) {
 		if (amount < 1 || scroll_only) {
			amount = 1;
		}
		edit_scroll(DOWN_DIR, (ISSET(SMOOTH_SCROLL) || scroll_only) ? amount : editwinrows / 2 + 1);

		edit_refresh_needed = true;
	}
	/* If we're above the last line of the edit window, update the line
	 * we were on before and the line we're on now.  The former needs to
	 * be redrawn if we're not on the first page, and the latter needs
	 * to be drawn unconditionally. */
	if (ISSET(SOFTWRAP) || openfile->current_y < editwinrows - 1) {
		if (need_vertical_update(0)) {
			update_line(openfile->current->prev, 0);
		}
		update_line(openfile->current, openfile->current_x);
	}
}

/* Move down one line. */
void do_down_void(void)
{
	do_down(false);
}

/* Scroll down one line without scrolling the cursor. */
void do_scroll_down(void)
{
	do_down(true);
}

/* Move left one character. */
void do_left(void)
{
	size_t pww_save = openfile->placewewant;

	if (openfile->current_x > 0) {
		openfile->current_x = move_mbleft(openfile->current->data, openfile->current_x);
	} else if (openfile->current != openfile->fileage) {
		do_up_void();
		openfile->current_x = strlen(openfile->current->data);
	}

	openfile->placewewant = xplustabs();

	if (need_horizontal_update(pww_save)) {
		update_line(openfile->current, openfile->current_x);
	}
}

/* Move right one character. */
void do_right(void)
{
	size_t pww_save = openfile->placewewant;

	assert(openfile->current_x <= strlen(openfile->current->data));

	if (openfile->current->data[openfile->current_x] != '\0') {
		openfile->current_x = move_mbright(openfile->current->data, openfile->current_x);
	} else if (openfile->current != openfile->filebot) {
		do_down_void();
		openfile->current_x = 0;
	}

	openfile->placewewant = xplustabs();

	if (need_horizontal_update(pww_save)) {
		update_line(openfile->current, openfile->current_x);
	}
}
