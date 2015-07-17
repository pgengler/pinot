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

using pinot::string;

static string prompt;
/* The prompt string used for statusbar questions. */
static size_t statusbar_x = (size_t)-1;
/* The cursor position in answer. */
static size_t statusbar_pww = (size_t)-1;
/* The place we want in answer. */
static size_t old_statusbar_x = (size_t)-1;
/* The old cursor position in answer, if any. */
static size_t old_pww = (size_t)-1;
/* The old place we want in answer, if any. */
static bool reset_statusbar_x = false;
/* Should we reset the cursor position at the statusbar prompt? */

/* Read in a character, interpret it as a shortcut or toggle if
 * necessary, and return it. Set ran_func to true if we ran a function
 * associated with a shortcut key, and set finished to true if we're
 * done after running or trying to run a function associated with a
 * shortcut key. refresh_func is the function we will call to refresh
 * the edit window. */
Key do_statusbar_input(bool *ran_func, bool *finished, void (*refresh_func)(void))
{
	*ran_func = false;
	*finished = false;

	/* Read in a character. */
	Key input = get_kbinput(bottomwin);

	/* Check for a shortcut in the current list. */
	const sc *s = get_shortcut(input);

	/* If we got a shortcut from the current list, or a "universal"
	 * statusbar prompt shortcut, set have_shortcut to true. */
	bool have_shortcut = (s != NULL);

	/* If we got a non-high-bit control key, a meta key sequence, or a
	 * function key, and it's not a shortcut or toggle, throw it out. */
	if (!have_shortcut && (input.has_control_key() || input.has_meta_key())) {
		beep();
	}

	if (have_shortcut) {
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
			do_statusbar_next_word(false);
		} else if (s->scfunc == do_prev_word_void) {
			do_statusbar_prev_word(false);
		} else if (s->scfunc == do_home) {
			do_statusbar_home();
		} else if (s->scfunc == do_end) {
			do_statusbar_end();
		} else if (s->scfunc == do_verbatim_input) {
			do_statusbar_verbatim_input();
		} else if (s->scfunc == do_delete) {
			do_statusbar_delete();
		} else if (s->scfunc == do_backspace) {
			do_statusbar_backspace();
		} else {
			/* Handle any other shortcut in the current menu, setting
			* ran_func to true if we try to run their associated
			* functions and setting finished to true to indicate
			* that we're done after running or trying to run their
			* associated functions. */

			const subnfunc *f = sctofunc((sc *) s);
			if (s->scfunc != NULL) {
				*ran_func = true;
				if (f && (!ISSET(VIEW_MODE) || (f->viewok)) && f->scfunc != do_gotolinecolumn_void) {
					f->scfunc();
				}
			}
			*finished = true;
		}
	} else {
		do_statusbar_output(string(input), false);
	}

	return input;
}

/* The user typed output_len multibyte characters.  Add them to the statusbar prompt,
 * filtering out all ASCII control characters if allow_cntrls is true. */
void do_statusbar_output(string output, bool allow_cntrls)
{
	for (auto ch : output) {
		/* If allow_cntrls is false, filter out an ASCII control character. */
		if (!allow_cntrls && ch.is_ascii_control()) {
			continue;
		}

		answer += ch;

		statusbar_x++;
	}

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

		if (statusbar_x == statusbar_x_save || statusbar_x == answer.length()) {
			statusbar_x = 0;
		}

		statusbar_pww = statusbar_xplustabs();
	} else {
		statusbar_x = 0;
		statusbar_pww = statusbar_xplustabs();
	}

	if (need_statusbar_update(pww_save)) {
		update_statusbar_line(answer, statusbar_x);
	}
}

/* Move to the end of the prompt text. */
void do_statusbar_end(void)
{
	size_t pww_save = statusbar_pww;

	statusbar_x = answer.length();
	statusbar_pww = statusbar_xplustabs();

	if (need_statusbar_update(pww_save)) {
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

		if (need_statusbar_update(pww_save)) {
			update_statusbar_line(answer, statusbar_x);
		}
	}
}

/* Move right one character. */
void do_statusbar_right(void)
{
	if (statusbar_x < answer.length()) {
		size_t pww_save = statusbar_pww;

		statusbar_x = move_mbright(answer, statusbar_x);
		statusbar_pww = statusbar_xplustabs();

		if (need_statusbar_update(pww_save)) {
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
		int char_buf_len = parse_mbchar(answer.c_str() + statusbar_x, NULL, NULL);
		answer = answer.substr(0, statusbar_x) + answer.substr(statusbar_x + char_buf_len);

		update_statusbar_line(answer, statusbar_x);
	}
}

/* Move text from the prompt into oblivion. */
void do_statusbar_cut_text(void)
{
	if (ISSET(CUT_TO_END)) {
		answer = answer.substr(0, statusbar_x);
	} else {
		answer = "";
		statusbar_x = 0;
		statusbar_pww = statusbar_xplustabs();
	}

	update_statusbar_line(answer, statusbar_x);
}

/* Move to the next word in the prompt text.  If allow_punct is true,
 * treat punctuation as part of a word.  Return true if we started on a
 * word, and false otherwise. */
bool do_statusbar_next_word(bool allow_punct)
{
	size_t pww_save = statusbar_pww;
	char *char_mb;
	int char_mb_len;
	bool end_line = false, started_on_word = false;

	char_mb = charalloc(mb_cur_max());

	/* Move forward until we find the character after the last letter of
	 * the current word. */
	while (!end_line) {
		char_mb_len = parse_mbchar(answer.c_str() + statusbar_x, char_mb, NULL);

		/* If we've found it, stop moving forward through the current line. */
		if (!is_word_mbchar(char_mb, allow_punct)) {
			break;
		}

		/* If we haven't found it, then we've started on a word, so set
		 * started_on_word to true. */
		started_on_word = true;

		if (answer[statusbar_x] == '\0') {
			end_line = true;
		} else {
			statusbar_x += char_mb_len;
		}
	}

	/* Move forward until we find the first letter of the next word. */
	if (answer[statusbar_x] == '\0') {
		end_line = true;
	} else {
		statusbar_x += char_mb_len;
	}

	while (!end_line) {
		char_mb_len = parse_mbchar(answer.c_str() + statusbar_x, char_mb, NULL);

		/* If we've found it, stop moving forward through the current line. */
		if (is_word_mbchar(char_mb, allow_punct)) {
			break;
		}

		if (answer[statusbar_x] == '\0') {
			end_line = true;
		} else {
			statusbar_x += char_mb_len;
		}
	}

	free(char_mb);

	statusbar_pww = statusbar_xplustabs();

	if (need_statusbar_update(pww_save)) {
		update_statusbar_line(answer, statusbar_x);
	}

	/* Return whether we started on a word. */
	return started_on_word;
}

/* Move to the previous word in the prompt text.  If allow_punct is
 * true, treat punctuation as part of a word.  Return true if we started
 * on a word, and false otherwise. */
bool do_statusbar_prev_word(bool allow_punct)
{
	size_t pww_save = statusbar_pww;
	char *char_mb;
	int char_mb_len;
	bool begin_line = false, started_on_word = false;

	char_mb = charalloc(mb_cur_max());

	/* Move backward until we find the character before the first letter
	 * of the current word. */
	while (!begin_line) {
		char_mb_len = parse_mbchar(answer.c_str() + statusbar_x, char_mb, NULL);

		/* If we've found it, stop moving backward through the current
		 * line. */
		if (!is_word_mbchar(char_mb, allow_punct)) {
			break;
		}

		/* If we haven't found it, then we've started on a word, so set
		 * started_on_word to true. */
		started_on_word = true;

		if (statusbar_x == 0) {
			begin_line = true;
		} else {
			statusbar_x = move_mbleft(answer, statusbar_x);
		}
	}

	/* Move backward until we find the last letter of the previous
	 * word. */
	if (statusbar_x == 0) {
		begin_line = true;
	} else {
		statusbar_x = move_mbleft(answer, statusbar_x);
	}

	while (!begin_line) {
		char_mb_len = parse_mbchar(answer.c_str() + statusbar_x, char_mb, NULL);

		/* If we've found it, stop moving backward through the current
		 * line. */
		if (is_word_mbchar(char_mb, allow_punct)) {
			break;
		}

		if (statusbar_x == 0) {
			begin_line = true;
		} else {
			statusbar_x = move_mbleft(answer, statusbar_x);
		}
	}

	/* If we've found it, move backward until we find the character
	 * before the first letter of the previous word. */
	if (!begin_line) {
		if (statusbar_x == 0) {
			begin_line = true;
		} else {
			statusbar_x = move_mbleft(answer, statusbar_x);
		}

		while (!begin_line) {
			char_mb_len = parse_mbchar(answer.c_str() + statusbar_x, char_mb, NULL);

			/* If we've found it, stop moving backward through the
			 * current line. */
			if (!is_word_mbchar(char_mb, allow_punct)) {
				break;
			}

			if (statusbar_x == 0) {
				begin_line = true;
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

	if (need_statusbar_update(pww_save)) {
		update_statusbar_line(answer, statusbar_x);
	}

	/* Return whether we started on a word. */
	return started_on_word;
}

/* Get verbatim input. */
void do_statusbar_verbatim_input(void)
{
	string kbinput = get_verbatim_kbinput(bottomwin);
	do_statusbar_output(kbinput, true);
}

/* Return the placewewant associated with statusbar_x, i.e. the
 * zero-based column position of the cursor.  The value will be no
 * smaller than statusbar_x. */
size_t statusbar_xplustabs(void)
{
	return strnlenpt(answer.c_str(), statusbar_x);
}

/* pinot scrolls horizontally within a line in chunks.  This function
 * returns the column number of the first character displayed in the
 * statusbar prompt when the cursor is at the given column with the
 * prompt ending at start_col.  Note that (0 <= column -
 * get_statusbar_page_start(column) < COLS). */
size_t get_statusbar_page_start(size_t start_col, size_t column)
{
	if (column == start_col || column < COLS - 1 || (start_col + 1) == COLS) {
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
void update_statusbar_line(string curranswer, size_t index)
{
	size_t start_col, page_start;

	assert(prompt != NULL && index <= curranswer.length());

	start_col = strlenpt(prompt) + 2;
	index = strnlenpt(curranswer.c_str(), index);
	page_start = get_statusbar_page_start(start_col, start_col + index);

	set_color(bottomwin, interface_colors[TITLE_BAR]);

	blank_statusbar();

	mvwaddnstr(bottomwin, 0, 0, prompt.c_str(), actual_x(prompt, COLS - 2));
	waddch(bottomwin, ':');
	waddch(bottomwin, (page_start == 0) ? ' ' : '$');

	string expanded = display_string(curranswer, page_start, COLS - start_col - 1, false);
	waddstr(bottomwin, expanded.c_str());

	clear_color(bottomwin, interface_colors[TITLE_BAR]);
	statusbar_pww = statusbar_xplustabs();
	reset_statusbar_cursor();
	wnoutrefresh(bottomwin);
}

/* Return true if we need an update after moving the cursor, and false otherwise.
 * We need one if pww_save and statusbar_pww are on different pages. */
bool need_statusbar_update(size_t pww_save)
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

/* Get a string of input at the statusbar prompt.  This should only be called from do_prompt(). */
FunctionPtr get_prompt_string(std::shared_ptr<Key>& actual, bool allow_tabs, bool allow_files, bool *list, const string& curranswer, History* history_list, void (*refresh_func)(void))
{
	std::shared_ptr<Key> kbinput, last_kbinput;
	bool ran_func, finished;
	FunctionPtr func;
	bool tabbed = false;
	/* Whether we've pressed Tab. */
	string history;
	/* The current history string. */
	string magichistory;
	/* The temporary string typed at the bottom of the history, if any. */
	size_t complete_len = 0;
	/* The length of the original string that we're trying to
	 * tab complete, if any. */

	answer = curranswer;

	/* If reset_statusbar_x is true, restore statusbar_x and
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

	if (statusbar_x == (size_t)-1 || statusbar_x > curranswer.length()) {
		statusbar_x = curranswer.length();
		statusbar_pww = statusbar_xplustabs();
	}

	DEBUG_LOG("get_prompt_string: answer = \"" << answer << "\", statusbar_x = " << statusbar_x);

	update_statusbar_line(answer, statusbar_x);
	reset_statusbar_cursor();

	/* Refresh the edit window and the statusbar before getting input. */
	wnoutrefresh(edit);
	wnoutrefresh(bottomwin);

	while (1) {
		kbinput = std::make_shared<Key>(do_statusbar_input(&ran_func, &finished, refresh_func));
		assert(statusbar_x <= answer.length());

		func = func_from_key(*kbinput);

		if (func == do_cancel || func == do_enter_void) {
			break;
		}

		if (func != do_tab) {
			tabbed = false;
		}

		if (func == do_tab) {
			if (history_list) {
				if (last_kbinput->format() != "Tab") {
					complete_len = answer.length();
				}

				if (complete_len > 0) {
					answer = history_list->find(answer, complete_len);
					statusbar_x = answer.length();
				}
			} else if (allow_tabs) {
				answer = input_tab(answer, allow_files, &statusbar_x, &tabbed, refresh_func, list);
			}

			update_statusbar_line(answer, statusbar_x);
		} else {
			if (func == get_history_older_void) {
				if (history_list) {
					/* If we're scrolling up at the bottom of the
					 * history list and answer isn't blank, save answer
					 * in magichistory. */
					if (history_list->at_newest() && answer != "") {
						magichistory = answer;
					}

					/* Get the older search from the history list and
					 * save it in answer.  If there is no older search,
					 * don't do anything. */
					if ((history = history_list->older()) != "") {
						answer = history;
						statusbar_x = answer.length();
					}

					update_statusbar_line(answer, statusbar_x);

					/* This key has a shortcut list entry when it's used
					 * to move to an older search, which means that
					 * finished has been set to true.  Set it back to
					 * false here, so that we aren't kicked out of the
					 * statusbar prompt. */
					finished = false;
				}
			} else if (func == get_history_newer_void) {
				if (history_list) {
					/* Get the newer search from the history list and
					 * save it in answer.  If there is no newer search,
					 * don't do anything. */
					if ((history = history_list->newer()) != "") {
						answer = history;
						statusbar_x = answer.length();
					}

					/* If, after scrolling down, we're at the bottom of
					 * the history list, answer is blank, and
					 * magichistory is set, save magichistory in
					 * answer. */
					if (history_list->at_newest() && answer != "" && magichistory != "") {
						answer = magichistory;
						statusbar_x = answer.length();
					}

					update_statusbar_line(answer, statusbar_x);

					/* This key has a shortcut list entry when it's used
					 * to move to a newer search, which means that
					 * finished has been set to true.  Set it back to
					 * false here, so that we aren't kicked out of the
					 * statusbar prompt. */
					finished = false;
				}
			} else if (func == do_help_void) {
				update_statusbar_line(answer, statusbar_x);

				/* This key has a shortcut list entry when it's used to
				 * go to the help browser or display a message
				 * indicating that help is disabled, which means that
				 * finished has been set to true.  Set it back to false
				 * here, so that we aren't kicked out of the statusbar
				 * prompt. */
				finished = false;
			}
		}

		/* If we have a shortcut with an associated function, break out
		 * if we're finished after running or trying to run the
		 * function. */
		if (finished) {
			break;
		}

		last_kbinput = kbinput;
		reset_statusbar_cursor();
		wnoutrefresh(bottomwin);
	}

	/* We've finished putting in an answer or run a normal shortcut's
	 * associated function, so reset statusbar_x and statusbar_pww.  If
	 * we've finished putting in an answer, reset the statusbar cursor
	 * position too. */
	if (func == do_cancel || func == do_enter_void || ran_func) {
		statusbar_x = old_statusbar_x;
		statusbar_pww = old_pww;

		if (!ran_func) {
			reset_statusbar_x = true;
		}
		/* Otherwise, we're still putting in an answer or a shortcut with
		 * an associated function, so leave the statusbar cursor position
		 * alone. */
	} else {
		reset_statusbar_x = false;
	}

	actual = kbinput;
	return func;
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
PromptResult do_prompt(bool allow_tabs, bool allow_files, int menu, std::shared_ptr<Key>& key, const string& curranswer, History* history_list, void (*refresh_func)(void), const char *msg, ...)
{
	va_list ap;
	PromptResult retval;
	bool list = false;

	char *new_prompt = new char[((COLS - 4) * mb_cur_max()) + 1];

	bottombars(menu);

	va_start(ap, msg);
	vsnprintf(new_prompt, (COLS - 4) * mb_cur_max(), msg, ap);
	va_end(ap);
	null_at(&new_prompt, actual_x(prompt, COLS - 4));
	prompt = string(new_prompt);
	delete[] new_prompt;

	auto func = get_prompt_string(key, allow_tabs, allow_files, &list, curranswer, history_list, refresh_func);

	prompt = "";

	/* We're done with the prompt, so save the statusbar cursor position. */
	old_statusbar_x = statusbar_x;
	old_pww = statusbar_pww;

	/* If we left the prompt via Cancel or Enter, set the return value properly. */
	if (func == do_cancel) {
		retval = PROMPT_ABORTED;
	} else if (func == do_enter_void) {
		retval = answer.empty() ? PROMPT_BLANK_STRING : PROMPT_ENTER_PRESSED;
	} else {
		retval = PROMPT_OTHER_KEY;
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

	reset_statusbar_x = true;
}

/* Ask a simple Yes/No (and optionally All) question, specified in msg,
 * on the statusbar.  Return 1 for Yes, 0 for No, 2 for All (if all is
 * true when passed in), and -1 for Cancel. */
YesNoPromptResult do_yesno_prompt(bool all, const char *msg)
{
	YesNoPromptResult ok = YESNO_PROMPT_UNKNOWN;
	int width = 16;
	const char *yesstr;		/* String of Yes characters accepted. */
	const char *nostr;		/* Same for No. */
	const char *allstr;		/* And All, surprise! */
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
		/* Temporary string for (translated) " Y", " N" and " A". */

		if (COLS < 32) {
			width = COLS / 2;
		}

		/* Clear the shortcut list from the bottom of the screen. */
		blank_bottombars();

		sprintf(shortstr, " %c", yesstr[0]);
		wmove(bottomwin, 1, 0);
		onekey(shortstr, _("Yes"), width);

		if (all) {
			shortstr[1] = allstr[0];
			wmove(bottomwin, 1, width);
			onekey(shortstr, _("All"), width);
		}

		shortstr[1] = nostr[0];
		wmove(bottomwin, 2, 0);
		onekey(shortstr, _("No"), width);

		wmove(bottomwin, 2, width);
		onekey("^C", _("Cancel"), width);
	}

	set_color(bottomwin, interface_colors[TITLE_BAR]);

	blank_statusbar();
	mvwaddnstr(bottomwin, 0, 0, msg, actual_x(msg, COLS - 1));

	clear_color(bottomwin, interface_colors[TITLE_BAR]);

	/* Refresh the edit window and the statusbar before getting input. */
	wnoutrefresh(edit);
	wnoutrefresh(bottomwin);

	do {
		currmenu = MYESNO;
		Key kbinput = get_kbinput(bottomwin);
		auto func = func_from_key(kbinput);
		string input(kbinput);

		if (func == do_cancel) {
			ok = YESNO_PROMPT_ABORTED;
		} else if (func == total_refresh) {
			total_redraw();
			continue;
		} else {
			/* Look for the kbinput in the Yes, No and (optionally) All strings. */
			if (input.find_first_of(yesstr) != string::npos) {
				ok = YESNO_PROMPT_YES;
			} else if (input.find_first_of(nostr) != string::npos) {
				ok = YESNO_PROMPT_NO;
			} else if (all && input.find_first_of(allstr) != string::npos) {
				ok = YESNO_PROMPT_ALL;
			}
		}
	} while (ok == YESNO_PROMPT_UNKNOWN);

	currmenu = oldmenu;
	return ok;
}
