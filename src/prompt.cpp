/**************************************************************************
 *   prompt.c                                                             *
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
#include <string.h>

static char *prompt = NULL;
/* The prompt string used for statusbar questions. */
static size_t statusbar_x = (size_t)-1;
/* The cursor position in answer. */
static size_t statusbar_pww = (size_t)-1;
/* The place we want in answer. */
static size_t old_statusbar_x = (size_t)-1;
/* The old cursor position in answer, if any. */
static size_t old_pww = (size_t)-1;
/* The old place we want in answer, if any. */
static bool reset_statusbar_x = FALSE;
/* Should we reset the cursor position at the statusbar prompt? */

/* Read in a character, interpret it as a shortcut or toggle if
 * necessary, and return it.  Set meta_key to TRUE if the character is a
 * meta sequence, set func_key to TRUE if the character is a function
 * key, set have_shortcut to TRUE if the character is a shortcut
 * key, set ran_func to TRUE if we ran a function associated with a
 * shortcut key, and set finished to TRUE if we're done after running
 * or trying to run a function associated with a shortcut key.  If
 * allow_funcs is FALSE, don't actually run any functions associated
 * with shortcut keys.  refresh_func is the function we will call to
 * refresh the edit window. */
Key do_statusbar_input(bool *meta_key, bool *func_key, bool *have_shortcut, bool *ran_func, bool *finished, bool allow_funcs, void (*refresh_func)(void))
{
	const sc *s;
	const subnfunc *f;

	*have_shortcut = FALSE;
	*ran_func = FALSE;
	*finished = FALSE;

	/* Read in a character. */
	Key input = get_kbinput(bottomwin);

	/* Check for a shortcut in the current list. */
	s = get_shortcut(currmenu, input);

	/* If we got a shortcut from the current list, or a "universal"
	 * statusbar prompt shortcut, set have_shortcut to TRUE. */
	*have_shortcut = (s != NULL);

	/* If we got a non-high-bit control key, a meta key sequence, or a
	 * function key, and it's not a shortcut or toggle, throw it out. */
	if (!*have_shortcut && (input.has_control_key() || input.has_meta_key())) {
		beep();
		*meta_key = FALSE;
		*func_key = FALSE;
	}

	if (allow_funcs && *have_shortcut) {
		if (s->scfunc == do_tab || s->scfunc == do_enter_void) {
			;
		} else if (s->scfunc == total_refresh) {
			total_statusbar_refresh(refresh_func);
		} else if (s->scfunc == do_cut_text_void) {
			do_statusbar_cut_text();
		} else if (s->scfunc == do_right) {
			do_statusbar_right();
		} else if (s->scfunc == do_left) {
			do_statusbar_left();
		} else if (s->scfunc == do_next_word_void) {
			do_statusbar_next_word(FALSE);
		} else if (s->scfunc == do_prev_word_void) {
			do_statusbar_prev_word(FALSE);
		} else if (s->scfunc == do_home) {
			do_statusbar_home();
		} else if (s->scfunc == do_end) {
			do_statusbar_end();
		} else if (s->scfunc == do_find_bracket) {
			do_statusbar_find_bracket();
		} else if (s->scfunc == do_verbatim_input) {
			do_statusbar_verbatim_input();
		} else if (s->scfunc == do_delete) {
			do_statusbar_delete();
		} else if (s->scfunc == do_backspace) {
			do_statusbar_backspace();
		} else {
			/* Handle the normal statusbar prompt shortcuts, setting
			* ran_func to TRUE if we try to run their associated
			* functions and setting finished to TRUE to indicate
			* that we're done after running or trying to run their
			* associated functions. */

			f = sctofunc((sc *) s);
			if (s->scfunc != 0 && s->execute == TRUE) {
				*ran_func = TRUE;
				if (f && (!ISSET(VIEW_MODE) || (f->viewok))) {
					f->scfunc();
				}
			}
			*finished = TRUE;
		}
	} else {
		do_statusbar_output(std::string(input),  FALSE);
	}

	return input;
}

/* The user typed output_len multibyte characters.  Add them to thestatusbar prompt,
 * filtering out all ASCII control characters if allow_cntrls is TRUE. */
void do_statusbar_output(std::string output, bool allow_cntrls)
{
	char *out = mallocstrcpy(NULL, output.c_str());
	do_statusbar_output(out, output.length(), allow_cntrls);
	free(out);
}

void do_statusbar_output(char *output, size_t output_len, bool allow_cntrls)
{
	size_t answer_len, i = 0;
	char *char_buf = charalloc(mb_cur_max());
	int char_buf_len;

	assert(answer != NULL);

	answer_len = strlen(answer);

	while (i < output_len) {
		/* If allow_cntrls is TRUE, convert nulls and newlines
		 * properly. */
		if (allow_cntrls) {
			/* Null to newline, if needed. */
			if (output[i] == '\0') {
				output[i] = '\n';
			}
		}

		/* Interpret the next multibyte character. */
		char_buf_len = parse_mbchar(output + i, char_buf, NULL);

		i += char_buf_len;

		/* If allow_cntrls is FALSE, filter out an ASCII control character. */
		if (!allow_cntrls && is_ascii_cntrl_char(*(output + i - char_buf_len))) {
			continue;
		}

		/* More dangerousness fun =) */
		answer = charealloc(answer, answer_len + (char_buf_len * 2));

		assert(statusbar_x <= answer_len);

		charmove(answer + statusbar_x + char_buf_len, answer + statusbar_x, answer_len - statusbar_x + char_buf_len);
		strncpy(answer + statusbar_x, char_buf, char_buf_len);
		answer_len += char_buf_len;

		statusbar_x += char_buf_len;
	}

	free(char_buf);

	statusbar_pww = statusbar_xplustabs();

	update_statusbar_line(answer, statusbar_x);
}

/* Move to the beginning of the prompt text.  If the SMART_HOME flag is
 * set, move to the first non-whitespace character of the prompt text if
 * we're not already there, or to the beginning of the prompt text if we
 * are. */
void do_statusbar_home(void)
{
	size_t pww_save = statusbar_pww;

	if (ISSET(SMART_HOME)) {
		size_t statusbar_x_save = statusbar_x;

		statusbar_x = indent_length(answer);

		if (statusbar_x == statusbar_x_save || statusbar_x == strlen(answer)) {
			statusbar_x = 0;
		}

		statusbar_pww = statusbar_xplustabs();
	} else {
		statusbar_x = 0;
		statusbar_pww = statusbar_xplustabs();
	}

	if (need_statusbar_horizontal_update(pww_save)) {
		update_statusbar_line(answer, statusbar_x);
	}
}

/* Move to the end of the prompt text. */
void do_statusbar_end(void)
{
	size_t pww_save = statusbar_pww;

	statusbar_x = strlen(answer);
	statusbar_pww = statusbar_xplustabs();

	if (need_statusbar_horizontal_update(pww_save)) {
		update_statusbar_line(answer, statusbar_x);
	}
}

/* Move left one character. */
void do_statusbar_left(void)
{
	if (statusbar_x > 0) {
		size_t pww_save = statusbar_pww;

		statusbar_x = move_mbleft(answer, statusbar_x);
		statusbar_pww = statusbar_xplustabs();

		if (need_statusbar_horizontal_update(pww_save)) {
			update_statusbar_line(answer, statusbar_x);
		}
	}
}

/* Move right one character. */
void do_statusbar_right(void)
{
	if (statusbar_x < strlen(answer)) {
		size_t pww_save = statusbar_pww;

		statusbar_x = move_mbright(answer, statusbar_x);
		statusbar_pww = statusbar_xplustabs();

		if (need_statusbar_horizontal_update(pww_save)) {
			update_statusbar_line(answer, statusbar_x);
		}
	}
}

/* Backspace over one character. */
void do_statusbar_backspace(void)
{
	if (statusbar_x > 0) {
		do_statusbar_left();
		do_statusbar_delete();
	}
}

/* Delete one character. */
void do_statusbar_delete(void)
{
	statusbar_pww = statusbar_xplustabs();

	if (answer[statusbar_x] != '\0') {
		int char_buf_len = parse_mbchar(answer + statusbar_x, NULL, NULL);
		size_t line_len = strlen(answer + statusbar_x);

		assert(statusbar_x < strlen(answer));

		charmove(answer + statusbar_x, answer + statusbar_x + char_buf_len, strlen(answer) - statusbar_x - char_buf_len + 1);

		null_at(&answer, statusbar_x + line_len - char_buf_len);

		update_statusbar_line(answer, statusbar_x);
	}
}

/* Move text from the prompt into oblivion. */
void do_statusbar_cut_text(void)
{
	assert(answer != NULL);

	if (ISSET(CUT_TO_END)) {
		null_at(&answer, statusbar_x);
	} else {
		null_at(&answer, 0);
		statusbar_x = 0;
		statusbar_pww = statusbar_xplustabs();
	}

	update_statusbar_line(answer, statusbar_x);
}

/* Move to the next word in the prompt text.  If allow_punct is TRUE,
 * treat punctuation as part of a word.  Return TRUE if we started on a
 * word, and FALSE otherwise. */
bool do_statusbar_next_word(bool allow_punct)
{
	size_t pww_save = statusbar_pww;
	char *char_mb;
	int char_mb_len;
	bool end_line = FALSE, started_on_word = FALSE;

	assert(answer != NULL);

	char_mb = charalloc(mb_cur_max());

	/* Move forward until we find the character after the last letter of
	 * the current word. */
	while (!end_line) {
		char_mb_len = parse_mbchar(answer + statusbar_x, char_mb, NULL);

		/* If we've found it, stop moving forward through the current line. */
		if (!is_word_mbchar(char_mb, allow_punct)) {
			break;
		}

		/* If we haven't found it, then we've started on a word, so set
		 * started_on_word to TRUE. */
		started_on_word = TRUE;

		if (answer[statusbar_x] == '\0') {
			end_line = TRUE;
		} else {
			statusbar_x += char_mb_len;
		}
	}

	/* Move forward until we find the first letter of the next word. */
	if (answer[statusbar_x] == '\0') {
		end_line = TRUE;
	} else {
		statusbar_x += char_mb_len;
	}

	while (!end_line) {
		char_mb_len = parse_mbchar(answer + statusbar_x, char_mb, NULL);

		/* If we've found it, stop moving forward through the current line. */
		if (is_word_mbchar(char_mb, allow_punct)) {
			break;
		}

		if (answer[statusbar_x] == '\0') {
			end_line = TRUE;
		} else {
			statusbar_x += char_mb_len;
		}
	}

	free(char_mb);

	statusbar_pww = statusbar_xplustabs();

	if (need_statusbar_horizontal_update(pww_save)) {
		update_statusbar_line(answer, statusbar_x);
	}

	/* Return whether we started on a word. */
	return started_on_word;
}

/* Move to the previous word in the prompt text.  If allow_punct is
 * TRUE, treat punctuation as part of a word.  Return TRUE if we started
 * on a word, and FALSE otherwise. */
bool do_statusbar_prev_word(bool allow_punct)
{
	size_t pww_save = statusbar_pww;
	char *char_mb;
	int char_mb_len;
	bool begin_line = FALSE, started_on_word = FALSE;

	assert(answer != NULL);

	char_mb = charalloc(mb_cur_max());

	/* Move backward until we find the character before the first letter
	 * of the current word. */
	while (!begin_line) {
		char_mb_len = parse_mbchar(answer + statusbar_x, char_mb, NULL);

		/* If we've found it, stop moving backward through the current
		 * line. */
		if (!is_word_mbchar(char_mb, allow_punct)) {
			break;
		}

		/* If we haven't found it, then we've started on a word, so set
		 * started_on_word to TRUE. */
		started_on_word = TRUE;

		if (statusbar_x == 0) {
			begin_line = TRUE;
		} else {
			statusbar_x = move_mbleft(answer, statusbar_x);
		}
	}

	/* Move backward until we find the last letter of the previous
	 * word. */
	if (statusbar_x == 0) {
		begin_line = TRUE;
	} else {
		statusbar_x = move_mbleft(answer, statusbar_x);
	}

	while (!begin_line) {
		char_mb_len = parse_mbchar(answer + statusbar_x, char_mb, NULL);

		/* If we've found it, stop moving backward through the current
		 * line. */
		if (is_word_mbchar(char_mb, allow_punct)) {
			break;
		}

		if (statusbar_x == 0) {
			begin_line = TRUE;
		} else {
			statusbar_x = move_mbleft(answer, statusbar_x);
		}
	}

	/* If we've found it, move backward until we find the character
	 * before the first letter of the previous word. */
	if (!begin_line) {
		if (statusbar_x == 0) {
			begin_line = TRUE;
		} else {
			statusbar_x = move_mbleft(answer, statusbar_x);
		}

		while (!begin_line) {
			char_mb_len = parse_mbchar(answer + statusbar_x, char_mb, NULL);

			/* If we've found it, stop moving backward through the
			 * current line. */
			if (!is_word_mbchar(char_mb, allow_punct)) {
				break;
			}

			if (statusbar_x == 0) {
				begin_line = TRUE;
			} else {
				statusbar_x = move_mbleft(answer, statusbar_x);
			}
		}

		/* If we've found it, move forward to the first letter of the
		 * previous word. */
		if (!begin_line) {
			statusbar_x += char_mb_len;
		}
	}

	free(char_mb);

	statusbar_pww = statusbar_xplustabs();

	if (need_statusbar_horizontal_update(pww_save)) {
		update_statusbar_line(answer, statusbar_x);
	}

	/* Return whether we started on a word. */
	return started_on_word;
}

/* Get verbatim input. */
void do_statusbar_verbatim_input(void)
{
	std::string kbinput = get_verbatim_kbinput(bottomwin);
	do_statusbar_output(kbinput, TRUE);
}

/* Search for a match to one of the two characters in bracket_set.  If
 * reverse is TRUE, search backwards for the leftmost bracket.
 * Otherwise, search forwards for the rightmost bracket.  Return TRUE if
 * we found a match, and FALSE otherwise. */
bool find_statusbar_bracket_match(bool reverse, const char *bracket_set)
{
	const char *rev_start = NULL, *found = NULL;

	assert(mbstrlen(bracket_set) == 2);

	/* rev_start might end up 1 character before the start or after the
	 * end of the line.  This won't be a problem because we'll skip over
	 * it below in that case. */
	rev_start = reverse ? answer + (statusbar_x - 1) : answer + (statusbar_x + 1);

	while (TRUE) {
		/* Look for either of the two characters in bracket_set.
		 * rev_start can be 1 character before the start or after the
		 * end of the line.  In either case, just act as though no match
		 * is found. */
		found = ((rev_start > answer && *(rev_start - 1) == '\0') ||
		         rev_start < answer) ? NULL : (reverse ?
		                                       mbrevstrpbrk(answer, bracket_set, rev_start) :
		                                       mbstrpbrk(rev_start, bracket_set));

		/* We've found a potential match. */
		if (found != NULL) {
			break;
		}

		/* We've reached the start or end of the statusbar text, so get out. */
		return FALSE;
	}

	/* We've definitely found something. */
	statusbar_x = found - answer;
	statusbar_pww = statusbar_xplustabs();

	return TRUE;
}

/* Search for a match to the bracket at the current cursor position, if
 * there is one. */
void do_statusbar_find_bracket(void)
{
	size_t statusbar_x_save, pww_save;
	const char *ch;
	/* The location in matchbrackets of the bracket at the current
	 * cursor position. */
	int ch_len;
	/* The length of ch in bytes. */
	const char *wanted_ch;
	/* The location in matchbrackets of the bracket complementing
	 * the bracket at the current cursor position. */
	int wanted_ch_len;
	/* The length of wanted_ch in bytes. */
	char *bracket_set;
	/* The pair of characters in ch and wanted_ch. */
	size_t i;
	/* Generic loop variable. */
	size_t matchhalf;
	/* The number of single-byte characters in one half of
	 * matchbrackets. */
	size_t mbmatchhalf;
	/* The number of multibyte characters in one half of
	 * matchbrackets. */
	size_t count = 1;
	/* The initial bracket count. */
	bool reverse;
	/* The direction we search. */
	char *found_ch;
	/* The character we find. */

	assert(mbstrlen(matchbrackets) % 2 == 0);

	ch = answer + statusbar_x;

	if (ch == '\0' || (ch = mbstrchr(matchbrackets, ch)) == NULL) {
		return;
	}

	/* Save where we are. */
	statusbar_x_save = statusbar_x;
	pww_save = statusbar_pww;

	/* If we're on an opening bracket, which must be in the first half
	 * of matchbrackets, we want to search forwards for a closing
	 * bracket.  If we're on a closing bracket, which must be in the
	 * second half of matchbrackets, we want to search backwards for an
	 * opening bracket. */
	matchhalf = 0;
	mbmatchhalf = mbstrlen(matchbrackets) / 2;

	for (i = 0; i < mbmatchhalf; i++)
		matchhalf += parse_mbchar(matchbrackets + matchhalf, NULL, NULL);

	reverse = ((ch - matchbrackets) >= matchhalf);

	/* If we're on an opening bracket, set wanted_ch to the character
	 * that's matchhalf characters after ch.  If we're on a closing
	 * bracket, set wanted_ch to the character that's matchhalf
	 * characters before ch. */
	wanted_ch = ch;

	while (mbmatchhalf > 0) {
		if (reverse)
			wanted_ch = matchbrackets + move_mbleft(matchbrackets, wanted_ch - matchbrackets);
		else {
			wanted_ch += move_mbright(wanted_ch, 0);
		}

		mbmatchhalf--;
	}

	ch_len = parse_mbchar(ch, NULL, NULL);
	wanted_ch_len = parse_mbchar(wanted_ch, NULL, NULL);

	/* Fill bracket_set in with the values of ch and wanted_ch. */
	bracket_set = charalloc((mb_cur_max() * 2) + 1);
	strncpy(bracket_set, ch, ch_len);
	strncpy(bracket_set + ch_len, wanted_ch, wanted_ch_len);
	null_at(&bracket_set, ch_len + wanted_ch_len);

	found_ch = charalloc(mb_cur_max() + 1);

	while (TRUE) {
		if (find_statusbar_bracket_match(reverse, bracket_set)) {
			/* If we found an identical bracket, increment count.  If we
			 * found a complementary bracket, decrement it. */
			parse_mbchar(answer + statusbar_x, found_ch, NULL);
			count += (strncmp(found_ch, ch, ch_len) == 0) ? 1 : -1;

			/* If count is zero, we've found a matching bracket.  Update
			 * the statusbar prompt and get out. */
			if (count == 0) {
				if (need_statusbar_horizontal_update(pww_save)) {
					update_statusbar_line(answer, statusbar_x);
				}
				break;
			}
		} else {
			/* We didn't find either an opening or closing bracket.
			 * Restore where we were, and get out. */
			statusbar_x = statusbar_x_save;
			statusbar_pww = pww_save;
			break;
		}
	}

	/* Clean up. */
	free(bracket_set);
	free(found_ch);
}

/* Return the placewewant associated with statusbar_x, i.e. the
 * zero-based column position of the cursor.  The value will be no
 * smaller than statusbar_x. */
size_t statusbar_xplustabs(void)
{
	return strnlenpt(answer, statusbar_x);
}

/* pinot scrolls horizontally within a line in chunks.  This function
 * returns the column number of the first character displayed in the
 * statusbar prompt when the cursor is at the given column with the
 * prompt ending at start_col.  Note that (0 <= column -
 * get_statusbar_page_start(column) < COLS). */
size_t get_statusbar_page_start(size_t start_col, size_t column)
{
	if (column == start_col || column < COLS - 1) {
		return 0;
	} else {
		return column - start_col - (column - start_col) % (COLS - start_col - 1);
	}
}

/* Put the cursor in the statusbar prompt at statusbar_x. */
void reset_statusbar_cursor(void)
{
	size_t start_col = strlenpt(prompt) + 2;
	size_t xpt = statusbar_xplustabs();

	wmove(bottomwin, 0, start_col + xpt - get_statusbar_page_start(start_col, start_col + xpt));
}

/* Repaint the statusbar when getting a character in
 * get_prompt_string().  The statusbar text line will be displayed
 * starting with curranswer[index]. */
void update_statusbar_line(const char *curranswer, size_t index)
{
	size_t start_col, page_start;
	char *expanded;

	assert(prompt != NULL && index <= strlen(curranswer));

	start_col = strlenpt(prompt) + 2;
	index = strnlenpt(curranswer, index);
	page_start = get_statusbar_page_start(start_col, start_col + index);

	wattron(bottomwin, reverse_attr);

	blank_statusbar();

	mvwaddnstr(bottomwin, 0, 0, prompt, actual_x(prompt, COLS - 2));
	waddch(bottomwin, ':');
	waddch(bottomwin, (page_start == 0) ? ' ' : '$');

	expanded = display_string(curranswer, page_start, COLS - start_col - 1, FALSE);
	waddstr(bottomwin, expanded);
	free(expanded);

	wattroff(bottomwin, reverse_attr);
	statusbar_pww = statusbar_xplustabs();
	reset_statusbar_cursor();
	wnoutrefresh(bottomwin);
}

/* Return TRUE if we need an update after moving horizontally, and FALSE
 * otherwise.  We need one if pww_save and statusbar_pww are on
 * different pages. */
bool need_statusbar_horizontal_update(size_t pww_save)
{
	size_t start_col = strlenpt(prompt) + 2;

	return get_statusbar_page_start(start_col, start_col + pww_save) != get_statusbar_page_start(start_col, start_col + statusbar_pww);
}

/* Unconditionally redraw the entire screen, and then refresh it using refresh_func(). */
void total_statusbar_refresh(void (*refresh_func)(void))
{
	total_redraw();
	refresh_func();
}

/* Get a string of input at the statusbar prompt.  This should only be
 * called from do_prompt(). */
const sc *get_prompt_string(std::shared_ptr<Key>& actual, bool allow_tabs, bool allow_files, const char *curranswer, bool *meta_key, bool *func_key, filestruct **history_list, void (*refresh_func)(void), int menu, bool *list)
{
	std::shared_ptr<Key> kbinput;
	bool have_shortcut, ran_func, finished;
	size_t curranswer_len;
	const sc *s;
	bool tabbed = FALSE;
	/* Whether we've pressed Tab. */
	char *history = NULL;
	/* The current history string. */
	char *magichistory = NULL;
	/* The temporary string typed at the bottom of the history, if
	 * any. */
	size_t complete_len = 0;
	/* The length of the original string that we're trying to
	 * tab complete, if any. */

	answer = mallocstrcpy(answer, curranswer);
	curranswer_len = strlen(answer);

	/* If reset_statusbar_x is TRUE, restore statusbar_x and
	 * statusbar_pww to what they were before this prompt.  Then, if
	 * statusbar_x is uninitialized or past the end of curranswer, put
	 * statusbar_x at the end of the string and update statusbar_pww
	 * based on it.  We do these things so that the cursor position
	 * stays at the right place if a prompt-changing toggle is pressed,
	 * or if this prompt was started from another prompt and we cancel
	 * out of it. */
	if (reset_statusbar_x) {
		statusbar_x = old_statusbar_x;
		statusbar_pww = old_pww;
	}

	if (statusbar_x == (size_t)-1 || statusbar_x > curranswer_len) {
		statusbar_x = curranswer_len;
		statusbar_pww = statusbar_xplustabs();
	}

	currmenu = menu;

	DEBUG_LOG("get_prompt_string: answer = \"" << answer << "\", statusbar_x = " << statusbar_x);

	update_statusbar_line(answer, statusbar_x);
	reset_statusbar_cursor();

	/* Refresh the edit window and the statusbar before getting input. */
	wnoutrefresh(edit);
	wnoutrefresh(bottomwin);

	while (1) {
		kbinput = std::make_shared<Key>(do_statusbar_input(meta_key, func_key, &have_shortcut, &ran_func, &finished, TRUE, refresh_func));
		assert(statusbar_x <= strlen(answer));

DEBUG_LOG("currmenu == 0x" << std::hex << currmenu << std::dec);
		s = get_shortcut(currmenu, *kbinput);

		if (s) {
			if (s->scfunc == do_cancel || s->scfunc == do_enter_void) {
				break;
			}
		}

		if (s && s->scfunc != do_tab) {
			tabbed = FALSE;
		}

		if (s && s->scfunc == do_tab) {
			if (history_list != NULL) {
/*
				if (last_kbinput != sc_seq_or(do_tab, PINOT_CONTROL_I)) {
					complete_len = strlen(answer);
				}
*/

				if (complete_len > 0) {
					answer = mallocstrcpy(answer, get_history_completion(history_list, answer, complete_len));
					statusbar_x = strlen(answer);
				}
			} else if (allow_tabs) {
				answer = input_tab(answer, allow_files, &statusbar_x, &tabbed, refresh_func, list);
			}

			update_statusbar_line(answer, statusbar_x);
		} else {
			if (s && s->scfunc == get_history_older_void) {
				if (history_list != NULL) {
					/* If we're scrolling up at the bottom of the
					 * history list and answer isn't blank, save answer
					 * in magichistory. */
					if ((*history_list)->next == NULL && answer[0] != '\0') {
						magichistory = mallocstrcpy(magichistory, answer);
					}

					/* Get the older search from the history list and
					 * save it in answer.  If there is no older search,
					 * don't do anything. */
					if ((history = get_history_older(history_list)) != NULL) {
						answer = mallocstrcpy(answer, history);
						statusbar_x = strlen(answer);
					}

					update_statusbar_line(answer, statusbar_x);

					/* This key has a shortcut list entry when it's used
					 * to move to an older search, which means that
					 * finished has been set to TRUE.  Set it back to
					 * FALSE here, so that we aren't kicked out of the
					 * statusbar prompt. */
					finished = FALSE;
				}
			} else if (s && s->scfunc == get_history_newer_void) {
				if (history_list != NULL) {
					/* Get the newer search from the history list and
					 * save it in answer.  If there is no newer search,
					 * don't do anything. */
					if ((history = get_history_newer(history_list)) != NULL) {
						answer = mallocstrcpy(answer, history);
						statusbar_x = strlen(answer);
					}

					/* If, after scrolling down, we're at the bottom of
					 * the history list, answer is blank, and
					 * magichistory is set, save magichistory in
					 * answer. */
					if ((*history_list)->next == NULL && *answer == '\0' && magichistory != NULL) {
						answer = mallocstrcpy(answer, magichistory);
						statusbar_x = strlen(answer);
					}

					update_statusbar_line(answer, statusbar_x);

					/* This key has a shortcut list entry when it's used
					 * to move to a newer search, which means that
					 * finished has been set to TRUE.  Set it back to
					 * FALSE here, so that we aren't kicked out of the
					 * statusbar prompt. */
					finished = FALSE;
				}
			} else if (s && s->scfunc == do_help_void) {
				update_statusbar_line(answer, statusbar_x);

				/* This key has a shortcut list entry when it's used to
				 * go to the help browser or display a message
				 * indicating that help is disabled, which means that
				 * finished has been set to TRUE.  Set it back to FALSE
				 * here, so that we aren't kicked out of the statusbar
				 * prompt. */
				finished = FALSE;
			}
		}

		/* If we have a shortcut with an associated function, break out
		 * if we're finished after running or trying to run the
		 * function. */
		if (finished) {
			break;
		}

		reset_statusbar_cursor();
		wnoutrefresh(bottomwin);
	}

	/* Set the current position in the history list to the bottom and
	 * free magichistory, if we need to. */
	if (history_list != NULL) {
		history_reset(*history_list);

		if (magichistory != NULL) {
			free(magichistory);
		}
	}


	/* We've finished putting in an answer or run a normal shortcut's
	 * associated function, so reset statusbar_x and statusbar_pww.  If
	 * we've finished putting in an answer, reset the statusbar cursor
	 * position too. */
	if (s) {
		if (s->scfunc ==  do_cancel || s->scfunc == do_enter_void || ran_func) {
			statusbar_x = old_statusbar_x;
			statusbar_pww = old_pww;

			if (!ran_func) {
				reset_statusbar_x = TRUE;
			}
			/* Otherwise, we're still putting in an answer or a shortcut with
			 * an associated function, so leave the statusbar cursor position
			 * alone. */
		} else {
			reset_statusbar_x = FALSE;
		}
	}

	actual = kbinput;
	return s;
}

/* Ask a question on the statusbar.  The prompt will be stored in the
 * static prompt, which should be NULL initially, and the answer will be
 * stored in the answer global.  Returns -1 on abort, -2 on a blank string,
 * 0 if enter was pressed, and 1 otherwise.
 * curranswer is any editable text that we want to put up by default,
 * and refresh_func is the function we want to call to refresh the edit
 * window.
 *
 * The allow_tabs parameter indicates whether we should allow tabs to be
 * interpreted.  The allow_files parameter indicates whether we should
 * allow all files (as opposed to just directories) to be tab
 * completed. */
int do_prompt(bool allow_tabs, bool allow_files, int menu, std::shared_ptr<Key>& key, const char *curranswer, bool *meta_key, bool *func_key, filestruct **history_list, void (*refresh_func)(void), const char *msg, ...)
{
	va_list ap;
	int retval = 0;
	const sc *s;
	bool list = FALSE;

	/* prompt has been freed and set to NULL unless the user resized
	 * while at the statusbar prompt. */
	if (prompt != NULL) {
		free(prompt);
	}

	prompt = charalloc(((COLS - 4) * mb_cur_max()) + 1);

	bottombars(menu);

	va_start(ap, msg);
	vsnprintf(prompt, (COLS - 4) * mb_cur_max(), msg, ap);
	va_end(ap);
	null_at(&prompt, actual_x(prompt, COLS - 4));

	s = get_prompt_string(key, allow_tabs, allow_files, curranswer, meta_key, func_key, history_list, refresh_func, menu, &list);

	free(prompt);
	prompt = NULL;

	/* We're done with the prompt, so save the statusbar cursor position. */
	old_statusbar_x = statusbar_x;
	old_pww = statusbar_pww;

	/* If we left the prompt via Cancel or Enter, set the return value properly. */
	if (s && s->scfunc ==  do_cancel) {
		retval = -1;
	} else if (s && s->scfunc == do_enter_void) {
		retval = (*answer == '\0') ? -2 : 0;
	} else {
		retval = 1;
	}

	blank_statusbar();
	wnoutrefresh(bottomwin);

	DEBUG_LOG("answer = \"" << answer << '"');

	/* If we've done tab completion, there might be a list of filename
	 * matches on the edit window at this point.  Make sure that they're
	 * cleared off. */
	if (list) {
		refresh_func();
	}

	return retval;
}

/* This function forces a reset of the statusbar cursor position.  It
 * should be called when we get out of all statusbar prompts. */
void do_prompt_abort(void)
{
	/* Uninitialize the old cursor position in answer. */
	old_statusbar_x = (size_t)-1;
	old_pww = (size_t)-1;

	reset_statusbar_x = TRUE;
}

/* Ask a simple Yes/No (and optionally All) question, specified in msg,
 * on the statusbar.  Return 1 for Yes, 0 for No, 2 for All (if all is
 * TRUE when passed in), and -1 for Cancel. */
int do_yesno_prompt(bool all, const char *msg)
{
	int ok = -2, width = 16;
	const char *yesstr;		/* String of Yes characters accepted. */
	const char *nostr;		/* Same for No. */
	const char *allstr;		/* And All, surprise! */
	const sc *s;
	int oldmenu = currmenu;

	assert(msg != NULL);

	/* yesstr, nostr, and allstr are strings of any length.  Each string
	 * consists of all single-byte characters accepted as valid
	 * characters for that value.  The first value will be the one
	 * displayed in the shortcuts. */
	/* TRANSLATORS: For the next three strings, if possible, specify
	 * the single-byte shortcuts for both your language and English.
	 * For example, in French: "OoYy" for "Oui". */
	yesstr = _("Yy");
	nostr = _("Nn");
	allstr = _("Aa");

	if (!ISSET(NO_HELP)) {
		char shortstr[3];
		/* Temp string for Yes, No, All. */

		if (COLS < 32) {
			width = COLS / 2;
		}

		/* Clear the shortcut list from the bottom of the screen. */
		blank_bottombars();

		sprintf(shortstr, " %c", yesstr[0]);
		wmove(bottomwin, 1, 0);
		onekey(shortstr, _("Yes"), width);

		if (all) {
			wmove(bottomwin, 1, width);
			shortstr[1] = allstr[0];
			onekey(shortstr, _("All"), width);
		}

		wmove(bottomwin, 2, 0);
		shortstr[1] = nostr[0];
		onekey(shortstr, _("No"), width);

		wmove(bottomwin, 2, 16);
		onekey("^C", _("Cancel"), width);
	}

	wattron(bottomwin, reverse_attr);

	blank_statusbar();
	mvwaddnstr(bottomwin, 0, 0, msg, actual_x(msg, COLS - 1));

	wattroff(bottomwin, reverse_attr);

	/* Refresh the edit window and the statusbar before getting input. */
	wnoutrefresh(edit);
	wnoutrefresh(bottomwin);

	do {
		currmenu = MYESNO;
		Key kbinput = get_kbinput(bottomwin);
		s = get_shortcut(currmenu, kbinput);
		std::string input(kbinput);

		if (s && s->scfunc ==  do_cancel) {
			ok = -1;
		} else if  (s && s->scfunc == total_refresh) {
			total_redraw();
			continue;
		} else {
			/* Look for the kbinput in the Yes, No and (optionally) All strings. */
			if (input.find_first_of(yesstr) != std::string::npos) {
				ok = 1;
			} else if (input.find_first_of(nostr) != std::string::npos) {
				ok = 0;
			} else if (all && input.find_first_of(allstr) != std::string::npos) {
				ok = 2;
			}
		}
	} while (ok == -2);

	currmenu = oldmenu;
	return ok;
}
