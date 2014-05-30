/**************************************************************************
 *   search.c                                                             *
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
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

static bool search_last_line = false;
/* Have we gone past the last line while searching? */
static bool history_changed = false;
/* Have any of the history lists changed? */
static bool regexp_compiled = false;
/* Have we compiled any regular expressions? */

/* Compile the regular expression regexp to see if it's valid.  Return
 * true if it is, or false otherwise. */
bool regexp_init(const char *regexp)
{
	int rc;

	assert(!regexp_compiled);

	rc = regcomp(&search_regexp, regexp, REG_EXTENDED | (ISSET(CASE_SENSITIVE) ? 0 : REG_ICASE));

	if (rc != 0) {
		size_t len = regerror(rc, &search_regexp, NULL, 0);
		char *str = charalloc(len);

		regerror(rc, &search_regexp, str, len);
		statusbar(_("Bad regex \"%s\": %s"), regexp, str);
		free(str);

		return false;
	}

	regexp_compiled = true;

	return true;
}

/* Decompile the compiled regular expression we used in the last
 * search, if any. */
void regexp_cleanup(void)
{
	if (regexp_compiled) {
		regexp_compiled = false;
		regfree(&search_regexp);
	}
}

/* Indicate on the statusbar that the string at str was not found by the
 * last search. */
void not_found_msg(const std::string& str)
{
	not_found_msg(str.c_str());
}

void not_found_msg(const char *str)
{
	char *disp;
	int numchars;

	assert(str != NULL);

	disp = display_string(str, 0, (COLS / 2) + 1, false);
	numchars = actual_x(disp, mbstrnlen(disp, COLS / 2));

	statusbar(_("\"%.*s%s\" not found"), numchars, disp, (disp[numchars] == '\0') ? "" : "...");

	free(disp);
}

/* Abort the current search or replace.  Clean up by displaying the main
 * shortcut list, updating the screen if the mark was on before, and
 * decompiling the compiled regular expression we used in the last
 * search, if any. */
void search_replace_abort(void)
{
	display_main_list();
	if (openfile->mark_set) {
		edit_refresh();
	}
	regexp_cleanup();
}

/* Set up the system variables for a search or replace.  If use_answer
 * is true, only set backupstring to answer.  Return -2 to run the
 * opposite program (search -> replace, replace -> search), return -1 if
 * the search should be canceled (due to Cancel, a blank search string,
 * Go to Line, or a failed regcomp()), return 0 on success, and return 1
 * on rerun calling program.
 *
 * replacing is true if we call from do_replace(), and false if called
 * from do_search(). */
int search_init(bool replacing, bool use_answer)
{
	std::shared_ptr<Key> key;
	char *buf;
	static std::string backupstring;
	/* The search string we'll be using. */

	/* If use_answer is true, set backupstring to answer and get out. */
	if (use_answer) {
		backupstring = answer;
		return 0;
	}

	/* We display the search prompt below.  If the user types a partial
	 * search string and then Replace or a toggle, we will return to
	 * do_search() or do_replace() and be called again.  In that case,
	 * we should put the same search string back up. */

	if (last_search != "") {
		auto disp = display_string(last_search, 0, COLS / 3, false);

		buf = charalloc(disp.length() + 7);
		/* We use (COLS / 3) here because we need to see more on the line. */
		sprintf(buf, " [%s%s]", disp.c_str(), (last_search.length() > COLS / 3) ? "..." : "");
	} else {
		buf = mallocstrcpy(NULL, "");
	}

	/* This is now one simple call.  It just does a lot. */
	PromptResult i = do_prompt(false,
	              true,
	              replacing ? MREPLACE : MWHEREIS, key, backupstring,
	              &search_history,
	              edit_refresh, "%s%s%s%s%s%s", _("Search"),
	              /* TRANSLATORS: This string is just a modifier for the search prompt; no grammar is implied. */
	              ISSET(CASE_SENSITIVE) ? _(" [Case Sensitive]") : "",
	              /* TRANSLATORS: This string is just a modifier for the search prompt; no grammar is implied. */
	              ISSET(USE_REGEXP) ? _(" [Regexp]") : "",
	              /* TRANSLATORS: This string is just a modifier for the search prompt; no grammar is implied. */
	              ISSET(BACKWARDS_SEARCH) ? _(" [Backwards]") : "", replacing ? (openfile->mark_set ? _(" (to replace) in selection") : _(" (to replace)")) : "", buf);

	fflush(stderr);

	/* Release buf now that we don't need it anymore. */
	free(buf);

	backupstring = "";

	/* Cancel any search, or just return with no previous search. */
	if (i == PROMPT_ABORTED || (i == PROMPT_BLANK_STRING && last_search == "") || (!replacing && i == PROMPT_ENTER_PRESSED && answer == "")) {
		statusbar(_("Cancelled"));
		return -1;
	} else {
		const sc *s = key ? get_shortcut(currmenu, *key) : nullptr;
		auto func = s ? s->scfunc : nullptr;

		if (i == PROMPT_BLANK_STRING || i == PROMPT_ENTER_PRESSED) {
			/* Use last_search if answer is an empty string, or answer if it isn't. */
			if (ISSET(USE_REGEXP) && !regexp_init((i == PROMPT_BLANK_STRING) ? last_search.c_str() : answer.c_str())) {
				return -1;
			}
		} else if (func == case_sens_void) {
			TOGGLE(CASE_SENSITIVE);
			backupstring = answer;
			return 1;
		} else if (func == backwards_void) {
			TOGGLE(BACKWARDS_SEARCH);
			backupstring = answer;
			return 1;
		} else if (func == regexp_void) {
			TOGGLE(USE_REGEXP);
			backupstring = answer;
			return 1;
		} else if (func == do_replace || func == no_replace_void) {
			backupstring = answer;
			return -2;	/* Call the opposite search function. */
		} else if (func == do_gotolinecolumn_void) {
			do_gotolinecolumn(openfile->current->lineno, openfile->placewewant + 1, true, true, false, true);
			/* Put answer up on the statusbar and fall through. */
			return 3;
		} else {
			return -1;
		}
	}

	return 0;
}

/* Look for needle, starting at (current, current_x).  If no_sameline is
 * true, skip over begin when looking for needle.  begin is the line
 * where we first started searching, at column begin_x.  The return
 * value specifies whether we found anything.  If we did, set needle_len
 * to the length of the string we found if it isn't NULL. */
bool findnextstr(
#ifdef ENABLE_SPELLER
	bool whole_word,
#endif
	bool no_sameline, const filestruct *begin, size_t begin_x, const std::string& needle, size_t *needle_len)
{
	return findnextstr(
#ifdef ENABLE_SPELLER
		whole_word,
#endif
		no_sameline, begin, begin_x, needle.c_str(), needle_len
	);
}

bool findnextstr(
#ifdef ENABLE_SPELLER
    bool whole_word,
#endif
    bool no_sameline, const filestruct *begin, size_t begin_x, const char *needle, size_t *needle_len)
{
	size_t found_len;
	/* The length of the match we find. */
	size_t current_x_find = 0;
	/* The location in the current line of the match we find. */
	ssize_t current_y_find = openfile->current_y;
	filestruct *fileptr = openfile->current;
	const char *rev_start = fileptr->data, *found = NULL;
	const subnfunc *f;
	time_t lastkbcheck = time(NULL);

	/* rev_start might end up 1 character before the start or after the
	 * end of the line.  This won't be a problem because strstrwrapper()
	 * will return immediately and say that no match was found, and
	 * rev_start will be properly set when the search continues on the
	 * previous or next line. */
	rev_start += ISSET(BACKWARDS_SEARCH) ? openfile->current_x - 1 : openfile->current_x + 1;

	/* Look for needle in the current line we're searching. */
	enable_nodelay();
	while (true) {
		if (time(NULL) - lastkbcheck > 1) {
			lastkbcheck = time(NULL);
			f = getfuncfromkey(edit);
			if (f && f->scfunc == do_cancel) {
				statusbar(_("Cancelled"));
				return false;
			}
		}

		found = strstrwrapper(fileptr->data, needle, rev_start);

		/* We've found a potential match. */
		if (found != NULL) {
#ifdef ENABLE_SPELLER
			bool found_whole = false;
			/* Is this potential match a whole word? */
#endif

			/* Set found_len to the length of the potential match. */
			found_len =
			    ISSET(USE_REGEXP) ?
			    regmatches[0].rm_eo - regmatches[0].rm_so :
			    strlen(needle);

#ifdef ENABLE_SPELLER
			/* If we're searching for whole words, see if this potential
			 * match is a whole word. */
			if (whole_word) {
				char *word = mallocstrncpy(NULL, found, found_len + 1);
				word[found_len] = '\0';

				found_whole = is_whole_word(found - fileptr->data, fileptr->data, word);
				free(word);
			}
#endif

			/* If we're searching for whole words and this potential
			 * match isn't a whole word, or if we're not allowed to find
			 * a match on the same line we started on and this potential
			 * match is on that line, continue searching. */
			if (
#ifdef ENABLE_SPELLER
			    (!whole_word || found_whole) &&
#endif
			    (!no_sameline || fileptr != openfile->current)) {
				break;
			}
		}

		/* We've finished processing the file, so get out. */
		if (search_last_line) {
			not_found_msg(needle);
			disable_nodelay();
			return false;
		}

		/* Move to the previous or next line in the file. */
		if (ISSET(BACKWARDS_SEARCH)) {
			fileptr = fileptr->prev;
			current_y_find--;
		} else {
			fileptr = fileptr->next;
			current_y_find++;
		}

		/* We've reached the start or end of the buffer, so wrap around. */
		if (fileptr == NULL) {
			if (ISSET(BACKWARDS_SEARCH)) {
				fileptr = openfile->filebot;
				current_y_find = editwinrows - 1;
			} else {
				fileptr = openfile->fileage;
				current_y_find = 0;
			}
			statusbar(_("Search Wrapped"));
		}

		/* We've reached the original starting line. */
		if (fileptr == begin) {
			search_last_line = true;
		}

		rev_start = fileptr->data;
		if (ISSET(BACKWARDS_SEARCH)) {
			rev_start += strlen(fileptr->data);
		}
	}

	/* We found an instance. */
	current_x_find = found - fileptr->data;

	/* Ensure we haven't wrapped around again! */
	if (search_last_line &&
	        ((!ISSET(BACKWARDS_SEARCH) && current_x_find > begin_x) ||
	         (ISSET(BACKWARDS_SEARCH) && current_x_find < begin_x))
	   ) {
		not_found_msg(needle);
		disable_nodelay();
		return false;
	}

	disable_nodelay();
	/* We've definitely found something. */
	openfile->current = fileptr;
	openfile->current_x = current_x_find;
	openfile->placewewant = xplustabs();
	openfile->current_y = current_y_find;

	/* needle_len holds the length of needle. */
	if (needle_len != NULL) {
		*needle_len = found_len;
	}

	return true;
}

/* Clear the flag indicating that a search reached the last line of the
 * file.  We need to do this just before a new search. */
void findnextstr_wrap_reset(void)
{
	search_last_line = false;
}

/* Search for a string. */
void do_search(void)
{
	filestruct *fileptr = openfile->current;
	size_t fileptr_x = openfile->current_x;
	size_t pww_save = openfile->placewewant;
	int i;
	bool didfind;

	i = search_init(false, false);

	if (i == -1) {
		/* Cancel, Go to Line, blank search string, or regcomp() failed. */
		search_replace_abort();
	} else if (i == -2) {
		/* Replace. */
		do_replace();
	} else if (i == 1) {
		/* Case Sensitive, Backwards, or Regexp search toggle. */
		do_search();
	}

	if (i != 0) {
		return;
	}

	/* If answer is now "", copy last_search into answer. */
	if (answer == "") {
		answer = last_search;
	} else {
		last_search = answer;
	}

	/* If answer is not "", add this search string to the search history list. */
	if (answer != "") {
		update_history(&search_history, answer);
	}

	findnextstr_wrap_reset();
	didfind = findnextstr(
#ifdef ENABLE_SPELLER
	              false,
#endif
	              false, openfile->current, openfile->current_x, answer, NULL);

	/* Check to see if there's only one occurrence of the string and
	 * we're on it now. */
	if (fileptr == openfile->current && fileptr_x == openfile->current_x && didfind) {
		/* Do the search again, skipping over the current line, if we're
		 * doing a bol and/or eol regex search ("^", "$", or "^$"), so
		 * that we find one only once per line.  We should only end up
		 * back at the same position if the string isn't found again, in
		 * which case it's the only occurrence. */
		if (ISSET(USE_REGEXP) && regexp_bol_or_eol(&search_regexp, last_search.c_str())) {
			didfind = findnextstr(
#ifdef ENABLE_SPELLER
			              false,
#endif
			              true, openfile->current,
			              openfile->current_x, answer, NULL);
			if (fileptr == openfile->current && fileptr_x == openfile->current_x && !didfind) {
				statusbar(_("This is the only occurrence"));
			}
		} else {
			statusbar(_("This is the only occurrence"));
		}
	}

	openfile->placewewant = xplustabs();
	edit_redraw(fileptr, pww_save);
	search_replace_abort();
}

/* Search for the last string without prompting. */
void do_research(void)
{
	filestruct *fileptr = openfile->current;
	size_t fileptr_x = openfile->current_x;
	size_t pww_save = openfile->placewewant;
	bool didfind;

	if (last_search != "") {
		/* Since answer is "", use last_search! */
		if (ISSET(USE_REGEXP) && !regexp_init(last_search.c_str())) {
			return;
		}

		findnextstr_wrap_reset();
		didfind = findnextstr(
#ifdef ENABLE_SPELLER
		              false,
#endif
		              false, openfile->current, openfile->current_x,
		              last_search.c_str(), NULL);

		/* Check to see if there's only one occurrence of the string and
		 * we're on it now. */
		if (fileptr == openfile->current && fileptr_x == openfile->current_x && didfind) {
			/* Do the search again, skipping over the current line, if
			 * we're doing a bol and/or eol regex search ("^", "$", or
			 * "^$"), so that we find one only once per line.  We should
			 * only end up back at the same position if the string isn't
			 * found again, in which case it's the only occurrence. */
			if (ISSET(USE_REGEXP) && regexp_bol_or_eol(&search_regexp, last_search.c_str())) {
				didfind = findnextstr(
#ifdef ENABLE_SPELLER
				              false,
#endif
				              true, openfile->current, openfile->current_x,
				              answer, NULL);
				if (fileptr == openfile->current && fileptr_x == openfile->current_x && !didfind) {
					statusbar(_("This is the only occurrence"));
				}
			} else {
				statusbar(_("This is the only occurrence"));
			}
		}
	} else {
		statusbar(_("No current search pattern"));
	}

	openfile->placewewant = xplustabs();
	edit_redraw(fileptr, pww_save);
	search_replace_abort();
}

int replace_regexp(char *string, bool create)
{
	/* We have a split personality here.  If create is false, just
	 * calculate the size of the replacement line (necessary because of
	 * subexpressions \1 to \9 in the replaced text). */

	const char *c = last_replace.c_str();
	size_t search_match_count = regmatches[0].rm_eo - regmatches[0].rm_so;
	size_t new_line_size = strlen(openfile->current->data) + 1 - search_match_count;

	/* Iterate through the replacement text to handle subexpression
	 * replacement using \1, \2, \3, etc. */
	while (*c != '\0') {
		int num = (*(c + 1) - '0');

		if (*c != '\\' || num < 1 || num > 9 || num > search_regexp.re_nsub) {
			if (create) {
				*string++ = *c;
			}
			c++;
			new_line_size++;
		} else {
			size_t i = regmatches[num].rm_eo - regmatches[num].rm_so;

			/* Skip over the replacement expression. */
			c += 2;

			/* But add the length of the subexpression to new_size. */
			new_line_size += i;

			/* And if create is true, append the result of the
			 * subexpression match to the new line. */
			if (create) {
				strncpy(string, openfile->current->data + openfile->current_x + regmatches[num].rm_so, i);
				string += i;
			}
		}
	}

	if (create) {
		*string = '\0';
	}

	return new_line_size;
}

char *replace_line(const char *needle)
{
	char *copy;
	size_t new_line_size, search_match_count;

	/* Calculate the size of the new line. */
	if (ISSET(USE_REGEXP)) {
		search_match_count = regmatches[0].rm_eo - regmatches[0].rm_so;
		new_line_size = replace_regexp(NULL, false);
	} else {
		search_match_count = strlen(needle);
		new_line_size = strlen(openfile->current->data) - search_match_count + answer.length() + 1;
	}

	/* Create the buffer. */
	copy = charalloc(new_line_size);

	/* The head of the original line. */
	strncpy(copy, openfile->current->data, openfile->current_x);

	/* The replacement text. */
	if (ISSET(USE_REGEXP)) {
		replace_regexp(copy + openfile->current_x, true);
	} else
		strcpy(copy + openfile->current_x, answer.c_str());

	/* The tail of the original line. */
	assert(openfile->current_x + search_match_count <= strlen(openfile->current->data));

	strcat(copy, openfile->current->data + openfile->current_x + search_match_count);

	return copy;
}

/* Step through each replace word and prompt user before replacing.
 * Parameters real_current and real_current_x are needed in order to
 * allow the cursor position to be updated when a word before the cursor
 * is replaced by a shorter word.
 *
 * needle is the string to seek.  We replace it with answer.  Return -1
 * if needle isn't found, else the number of replacements performed.  If
 * canceled isn't NULL, set it to true if we canceled. */
ssize_t do_replace_loop(
#ifdef ENABLE_SPELLER
    bool whole_word,
#endif
    bool *canceled, const filestruct *real_current, size_t
    *real_current_x, const char *needle)
{
	ssize_t numreplaced = -1;
	size_t match_len;
	bool replaceall = false;
	/* The starting-line match and bol/eol regex flags. */
	bool begin_line = false, bol_or_eol = false;
	bool old_mark_set = openfile->mark_set;
	filestruct *edittop_save = openfile->edittop, *top, *bot;
	size_t top_x, bot_x;
	bool right_side_up = false;
	/* true if (mark_begin, mark_begin_x) is the top of the mark,
	 * false if (current, current_x) is. */

	if (old_mark_set) {
		/* If the mark is on, partition the filestruct so that it
		 * contains only the marked text, set edittop to the top of the
		 * partition, turn the mark off, and refresh the screen. */
		mark_order((const filestruct **)&top, &top_x, (const filestruct **)&bot, &bot_x, &right_side_up);
		filepart = partition_filestruct(top, top_x, bot, bot_x);
		openfile->edittop = openfile->fileage;
		openfile->mark_set = false;
		reset_multis(openfile->current, true);
		edit_refresh();
	}

	if (canceled != NULL) {
		*canceled = false;
	}

	findnextstr_wrap_reset();
	while (findnextstr(
#ifdef ENABLE_SPELLER
	            whole_word,
#endif
	            /* We should find a bol and/or eol regex only once per line.  If
	             * the bol_or_eol flag is set, it means that the last search
	             * found one on the beginning line, so we should skip over the
	             * beginning line when doing this search. */
	            bol_or_eol
	            , real_current, *real_current_x, needle, &match_len)) {
		int i = 0;

		/* If the bol_or_eol flag is set, we've found a match on the
		 * beginning line already, and we're still on the beginning line
		 * after the search, it means that we've wrapped around, so
		 * we're done. */
		if (bol_or_eol && begin_line && openfile->current == real_current) {
			break;
		}
		/* Otherwise, set the begin_line flag if we've found a match on
		 * the beginning line, reset the bol_or_eol flag, and
		 * continue. */
		else {
			if (openfile->current == real_current) {
				begin_line = true;
			}
			bol_or_eol = false;
		}

		if (!replaceall) {
			edit_refresh();
		}

		/* Indicate that we found the search string. */
		if (numreplaced == -1) {
			numreplaced = 0;
		}

		if (!replaceall) {
			size_t xpt = xplustabs();
			char *exp_word = display_string(openfile->current->data, xpt, strnlenpt(openfile->current->data, openfile->current_x + match_len) - xpt, false);

			curs_set(0);

			do_replace_highlight(true, exp_word);

			i = do_yesno_prompt(true, _("Replace this instance?"));

			do_replace_highlight(false, exp_word);

			free(exp_word);

			curs_set(1);

			if (i == -1) {	/* We canceled the replace. */
				if (canceled != NULL) {
					*canceled = true;
				}
				break;
			}
		}

		/* Set the bol_or_eol flag if we're doing a bol and/or eol regex
		 * replace ("^", "$", or "^$"). */
		if (ISSET(USE_REGEXP) && regexp_bol_or_eol(&search_regexp, needle)) {
			bol_or_eol = true;
		}

		if (i > 0 || replaceall) {	/* Yes, replace it!!!! */
			char *copy;
			size_t length_change;

			update_undo(REPLACE);

			if (i == 2) {
				replaceall = true;
			}

			copy = replace_line(needle);

			length_change = strlen(copy) - strlen(openfile->current->data);

			/* If the mark was on and (mark_begin, mark_begin_x) was the
			 * top of it, don't change mark_begin_x. */
			if (!old_mark_set || !right_side_up) {
				/* Keep mark_begin_x in sync with the text changes. */
				if (openfile->current == openfile->mark_begin && openfile->mark_begin_x > openfile->current_x) {
					if (openfile->mark_begin_x < openfile->current_x + match_len) {
						openfile->mark_begin_x = openfile->current_x;
					} else {
						openfile->mark_begin_x += length_change;
					}
				}
			}

			/* If the mark was on and (current, current_x) was the top
			 * of it, don't change real_current_x. */
			if (!old_mark_set || right_side_up) {
				/* Keep real_current_x in sync with the text changes. */
				if (openfile->current == real_current && openfile->current_x <= *real_current_x) {
					if (*real_current_x < openfile->current_x + match_len) {
						*real_current_x = openfile->current_x + match_len;
					}
					*real_current_x += length_change;
				}
			}

			/* Set the cursor at the last character of the replacement
			 * text, so searching will resume after the replacement
			 * text.  Note that current_x might be set to (size_t)-1
			 * here. */
			if (!ISSET(BACKWARDS_SEARCH)) {
				openfile->current_x += match_len + length_change - 1;
			}

			/* Cleanup. */
			openfile->totsize += mbstrlen(copy) - mbstrlen(openfile->current->data);
			free(openfile->current->data);
			openfile->current->data = copy;

			reset_multis(openfile->current, true);
			edit_refresh();
			if (!replaceall) {
				/* If color syntaxes are available and turned on, we
				 * need to call edit_refresh(). */
				if (!openfile->colorstrings.empty() && !ISSET(NO_COLOR_SYNTAX)) {
					edit_refresh();
				} else {
					update_line(openfile->current, openfile->current_x);
				}
			}

			set_modified();
			numreplaced++;
		}
	}

	if (old_mark_set) {
		/* If the mark was on, unpartition the filestruct so that it
		 * contains all the text again, set edittop back to what it was
		 * before, turn the mark back on, and refresh the screen. */
		unpartition_filestruct(&filepart);
		openfile->edittop = edittop_save;
		openfile->mark_set = true;
		edit_refresh();
	}

	/* If the NO_NEWLINES flag isn't set, and text has been added to the
	 * magicline, make a new magicline. */
	if (!ISSET(NO_NEWLINES) && openfile->filebot->data[0] != '\0') {
		new_magicline();
	}

	return numreplaced;
}

/* Replace a string. */
void do_replace(void)
{
	filestruct *edittop_save, *begin;
	size_t begin_x, pww_save;
	ssize_t numreplaced;

	if (ISSET(VIEW_MODE)) {
		print_view_warning();
		search_replace_abort();
		return;
	}

	int search_i = search_init(true, false);
	if (search_i == -1) {
		/* Cancel, Go to Line, blank search string, or regcomp() failed. */
		search_replace_abort();
		return;
	} else if (search_i == -2) {
		/* No Replace. */
		do_search();
		return;
	} else if (search_i == 1) {
		/* Case Sensitive, Backwards, or Regexp search toggle. */
		do_replace();
	}

	if (search_i != 0) {
		return;
	}

	/* If answer is not "", add answer to the search history list and
	 * copy answer into last_search. */
	if (answer[0] != '\0') {
		update_history(&search_history, answer);
		last_search = answer;
	}

	last_replace = "";

	std::shared_ptr<Key> key;
	PromptResult i = do_prompt(false,
	              true,
	              MREPLACEWITH, key, last_replace.c_str(),
	              &replace_history,
	              edit_refresh, _("Replace with"));

	/* Add this replace string to the replace history list.  i == 0
	 * means that the string is not "". */
	if (i == PROMPT_ENTER_PRESSED) {
		update_history(&replace_history, answer);
	}

	if (i != PROMPT_ENTER_PRESSED && i != PROMPT_BLANK_STRING) {
		if (i == PROMPT_ABORTED) {  /* Cancel. */
			if (last_replace != "") {
				answer = last_replace;
			}
			statusbar(_("Cancelled"));
		}
		search_replace_abort();
		return;
	}

	last_replace = answer;

	/* Save where we are. */
	edittop_save = openfile->edittop;
	begin = openfile->current;
	begin_x = openfile->current_x;
	pww_save = openfile->placewewant;

	numreplaced = do_replace_loop(
#ifdef ENABLE_SPELLER
	                  false,
#endif
	                  NULL, begin, &begin_x, last_search.c_str());

	/* Restore where we were. */
	openfile->edittop = edittop_save;
	openfile->current = begin;
	openfile->current_x = begin_x;
	openfile->placewewant = pww_save;

	edit_refresh();

	if (numreplaced >= 0) {
		statusbar(P_("Replaced %lu occurrence", "Replaced %lu occurrences", (unsigned long)numreplaced), (unsigned long)numreplaced);
	}

	search_replace_abort();
}

/* Go to the specified line and x position. */
void goto_line_posx(ssize_t line, size_t pos_x)
{
	for (openfile->current = openfile->fileage; openfile->current != openfile->filebot && openfile->current->next != NULL && line > 1; line--) {
		openfile->current = openfile->current->next;
	}

	openfile->current_x = pos_x;
	openfile->placewewant = xplustabs();

	edit_refresh_needed = TRUE;
	edit_refresh();
}

/* Go to the specified line and column, or ask for them if interactive
 * is true.  Save the x-coordinate and y-coordinate if save_pos is true.
 * Update the screen afterwards if allow_update is true.  Note that both
 * the line and column numbers should be one-based. */
void do_gotolinecolumn(ssize_t line, ssize_t column, bool use_answer, bool interactive, bool save_pos, bool allow_update)
{
	const sc *s;

	if (interactive) {
		char *ans = mallocstrcpy(NULL, answer.c_str());

		/* Ask for the line and column. */
		std::shared_ptr<Key> key;
		PromptResult i = do_prompt(false,
		                  true,
		                  MGOTOLINE, key, use_answer ? ans : "",
		                  NULL,
		                  edit_refresh, _("Enter line number, column number"));

		free(ans);

		/* Cancel, or Enter with blank string. */
		if (i == PROMPT_BLANK_STRING || i == PROMPT_ABORTED) {
			statusbar(_("Cancelled"));
			display_main_list();
			return;
		}


		s = get_shortcut(currmenu, *key);
		if (s && s->scfunc ==  gototext_void) {
			/* Keep answer up on the statusbar. */
			search_init(true, true);

			do_search();
			return;
		}

		bool relative_move = false;
		if (answer[0] == '+' || answer[0] == '-') {
			relative_move = true;
		}

		/* Do a bounds check.  Display a warning on an out-of-bounds
		 * line or column number only if we hit Enter at the statusbar
		 * prompt. */
		if (!parse_line_column(answer, &line, &column) || column < 1) {
			if (i == 0) {
				statusbar(_("Invalid line or column number"));
			}
			display_main_list();
			return;
		}

		if (relative_move) {
			line = openfile->current->lineno + line;
			if (line < 1) {
				line = 1;
			}
		}

	} else {
		if (line < 1) {
			line = openfile->current->lineno;
		}

		if (column < 1) {
			column = openfile->placewewant + 1;
		}
	}

	for (openfile->current = openfile->fileage; openfile->current != openfile->filebot && line > 1; line--) {
		openfile->current = openfile->current->next;
	}

	openfile->current_x = actual_x(openfile->current->data, column - 1);
	openfile->placewewant = column - 1;

	/* Put the top line of the edit window in range of the current line.
	 * If save_pos is true, don't change the cursor position when doing it. */
	edit_update(save_pos ? NONE : CENTER);

	/* If allow_update is true, update the screen. */
	if (allow_update) {
		edit_refresh();
	}

	display_main_list();
}

/* Go to the specified line and column, asking for them beforehand. */
void do_gotolinecolumn_void(void)
{
	do_gotolinecolumn(openfile->current->lineno, openfile->placewewant + 1, false, true, false, true);
}

#ifdef ENABLE_SPELLER
/* Go to the line with the number specified in pos_line, the
 * x-coordinate specified in pos_x, the y-coordinate specified in pos_y,
 * and the place we want specified in pos_pww. */
void do_gotopos(ssize_t pos_line, size_t pos_x, ssize_t pos_y, size_t pos_pww)
{
	/* Since do_gotolinecolumn() resets the x-coordinate but not the
	 * y-coordinate, set the coordinates up this way. */
	openfile->current_y = pos_y;
	do_gotolinecolumn(pos_line, pos_x + 1, false, false, true, true);

	/* Set the rest of the coordinates up. */
	openfile->placewewant = pos_pww;
	update_line(openfile->current, pos_x);
}
#endif

/* Search for a match to one of the two characters in bracket_set.  If
 * reverse is true, search backwards for the leftmost bracket.
 * Otherwise, search forwards for the rightmost bracket.  Return true if
 * we found a match, and false otherwise. */
bool find_bracket_match(bool reverse, const char *bracket_set)
{
	filestruct *fileptr = openfile->current;
	const char *rev_start = NULL, *found = NULL;
	ssize_t current_y_find = openfile->current_y;

	assert(mbstrlen(bracket_set) == 2);

	/* rev_start might end up 1 character before the start or after the
	 * end of the line.  This won't be a problem because we'll skip over
	 * it below in that case, and rev_start will be properly set when
	 * the search continues on the previous or next line. */
	rev_start = reverse ? fileptr->data + (openfile->current_x - 1) : fileptr->data + (openfile->current_x + 1);

	/* Look for either of the two characters in bracket_set.  rev_start
	 * can be 1 character before the start or after the end of the line.
	 * In either case, just act as though no match is found. */
	while (true) {
		found = ((rev_start > fileptr->data && *(rev_start - 1) ==
		          '\0') || rev_start < fileptr->data) ? NULL : (reverse ?
		                  mbrevstrpbrk(fileptr->data, bracket_set, rev_start) :
		                  mbstrpbrk(rev_start, bracket_set));

		/* We've found a potential match. */
		if (found != NULL) {
			break;
		}

		if (reverse) {
			fileptr = fileptr->prev;
			current_y_find--;
		} else {
			fileptr = fileptr->next;
			current_y_find++;
		}

		/* We've reached the start or end of the buffer, so get out. */
		if (fileptr == NULL) {
			return false;
		}

		rev_start = fileptr->data;
		if (reverse) {
			rev_start += strlen(fileptr->data);
		}
	}

	/* We've definitely found something. */
	openfile->current = fileptr;
	openfile->current_x = found - fileptr->data;
	openfile->placewewant = xplustabs();
	openfile->current_y = current_y_find;

	return true;
}

/* Search for a match to the bracket at the current cursor position, if
 * there is one. */
void do_find_bracket(void)
{
	filestruct *current_save;
	size_t current_x_save, pww_save;
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

	ch = openfile->current->data + openfile->current_x;

	if (*ch == '\0' || (ch = mbstrchr(matchbrackets, ch)) == NULL) {
		statusbar(_("Not a bracket"));
		return;
	}

	/* Save where we are. */
	current_save = openfile->current;
	current_x_save = openfile->current_x;
	pww_save = openfile->placewewant;

	/* If we're on an opening bracket, which must be in the first half
	 * of matchbrackets, we want to search forwards for a closing
	 * bracket.  If we're on a closing bracket, which must be in the
	 * second half of matchbrackets, we want to search backwards for an
	 * opening bracket. */
	matchhalf = 0;
	mbmatchhalf = mbstrlen(matchbrackets) / 2;

	for (i = 0; i < mbmatchhalf; i++) {
		matchhalf += parse_mbchar(matchbrackets + matchhalf, NULL, NULL);
	}

	reverse = ((ch - matchbrackets) >= matchhalf);

	/* If we're on an opening bracket, set wanted_ch to the character
	 * that's matchhalf characters after ch.  If we're on a closing
	 * bracket, set wanted_ch to the character that's matchhalf
	 * characters before ch. */
	wanted_ch = ch;

	while (mbmatchhalf > 0) {
		if (reverse) {
			wanted_ch = matchbrackets + move_mbleft(matchbrackets, wanted_ch - matchbrackets);
		} else {
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

	while (true) {
		if (find_bracket_match(reverse, bracket_set)) {
			/* If we found an identical bracket, increment count.  If we
			 * found a complementary bracket, decrement it. */
			parse_mbchar(openfile->current->data + openfile->current_x, found_ch, NULL);
			count += (strncmp(found_ch, ch, ch_len) == 0) ? 1 : -1;

			/* If count is zero, we've found a matching bracket.  Update
			 * the screen and get out. */
			if (count == 0) {
				edit_redraw(current_save, pww_save);
				break;
			}
		} else {
			/* We didn't find either an opening or closing bracket.
			 * Indicate this, restore where we were, and get out. */
			statusbar(_("No matching bracket"));
			openfile->current = current_save;
			openfile->current_x = current_x_save;
			openfile->placewewant = pww_save;
			break;
		}
	}

	/* Clean up. */
	free(bracket_set);
	free(found_ch);
}

/* Indicate whether any of the history lists have changed. */
bool history_has_changed(void)
{
	return history_changed;
}

/* Initialize the search and replace history lists. */
void history_init(void)
{
	search_history = make_new_node(NULL);
	search_history->data = mallocstrcpy(NULL, "");
	searchage = search_history;
	searchbot = search_history;

	replace_history = make_new_node(NULL);
	replace_history->data = mallocstrcpy(NULL, "");
	replaceage = replace_history;
	replacebot = replace_history;
}

/* Set the current position in the history list h to the bottom. */
void history_reset(const filestruct *h)
{
	if (h == search_history) {
		search_history = searchbot;
	} else if (h == replace_history) {
		replace_history = replacebot;
	}
}

/* Return the first node containing the first len characters of the
 * string s in the history list, starting at h_start and ending at
 * h_end, or NULL if there isn't one. */
filestruct *find_history(const filestruct *h_start, const filestruct *h_end, const char *s, size_t len)
{
	const filestruct *p;

	for (p = h_start; p != h_end->next && p != NULL; p = p->next) {
		if (strncmp(s, p->data, len) == 0) {
			return (filestruct *)p;
		}
	}

	return NULL;
}

/* Update a history list.  h should be the current position in the list. */
void update_history(filestruct **h, const std::string& s)
{
	update_history(h, s.c_str());
}

void update_history(filestruct **h, const char *s)
{
	filestruct **hage = NULL, **hbot = NULL, *p;

	assert(h != NULL && s != NULL);

	if (*h == search_history) {
		hage = &searchage;
		hbot = &searchbot;
	} else if (*h == replace_history) {
		hage = &replaceage;
		hbot = &replacebot;
	}

	assert(hage != NULL && hbot != NULL);

	/* If this string is already in the history, delete it. */
	p = find_history(*hage, *hbot, s, strlen(s));

	if (p != NULL) {
		filestruct *foo, *bar;

		/* If the string is at the beginning, move the beginning down to
		 * the next string. */
		if (p == *hage) {
			*hage = (*hage)->next;
		}

		/* Delete the string. */
		foo = p;
		bar = p->next;
		unlink_node(foo);
		delete_node(foo);
		if (bar != NULL) {
			renumber(bar);
		}
	}

	/* If the history is full, delete the beginning entry to make room
	 * for the new entry at the end.  We assume that MAX_SEARCH_HISTORY
	 * is greater than zero. */
	if ((*hbot)->lineno == MAX_SEARCH_HISTORY + 1) {
		filestruct *foo = *hage;

		*hage = (*hage)->next;
		unlink_node(foo);
		delete_node(foo);
		renumber(*hage);
	}

	/* Add the new entry to the end. */
	(*hbot)->data = mallocstrcpy((*hbot)->data, s);
	splice_node(*hbot, make_new_node(*hbot), (*hbot)->next);
	*hbot = (*hbot)->next;
	(*hbot)->data = mallocstrcpy(NULL, "");

	/* Indicate that the history's been changed. */
	history_changed = true;

	/* Set the current position in the list to the bottom. */
	*h = *hbot;
}

/* Move h to the string in the history list just before it, and return
 * that string.  If there isn't one, don't move h and return NULL. */
char *get_history_older(filestruct **h)
{
	assert(h != NULL);

	if ((*h)->prev == NULL) {
		return NULL;
	}

	*h = (*h)->prev;

	return (*h)->data;
}

/* Move h to the string in the history list just after it, and return
 * that string.  If there isn't one, don't move h and return NULL. */
char *get_history_newer(filestruct **h)
{
	assert(h != NULL);

	if ((*h)->next == NULL) {
		return NULL;
	}

	*h = (*h)->next;

	return (*h)->data;
}

/* More placeholders */
void get_history_newer_void(void)
{
	;
}
void get_history_older_void(void)
{
	;
}

/* Move h to the next string that's a tab completion of the string s,
 * looking at only the first len characters of s, and return that string.
 *  If there isn't one, or if len is 0, don't move h and return s. */
std::string get_history_completion(filestruct **h, const std::string& s, size_t len)
{
	char *result_cstr = get_history_completion(h, s.c_str(), len);
	std::string result(result_cstr);
	free(result_cstr);

	return result;
}

char *get_history_completion(filestruct **h, const char *s, size_t len)
{
	assert(s != NULL);

	if (len > 0) {
		filestruct *hage = NULL, *hbot = NULL, *p;

		assert(h != NULL);

		if (*h == search_history) {
			hage = searchage;
			hbot = searchbot;
		} else if (*h == replace_history) {
			hage = replaceage;
			hbot = replacebot;
		}

		assert(hage != NULL && hbot != NULL);

		/* Search the history list from the current position to the
		 * bottom for a match of len characters.  Skip over an exact
		 * match. */
		p = find_history((*h)->next, hbot, s, len);

		while (p != NULL && strcmp(p->data, s) == 0) {
			p = find_history(p->next, hbot, s, len);
		}

		if (p != NULL) {
			*h = p;
			return (*h)->data;
		}

		/* Search the history list from the top to the current position
		 * for a match of len characters.  Skip over an exact match. */
		p = find_history(hage, *h, s, len);

		while (p != NULL && strcmp(p->data, s) == 0) {
			p = find_history(p->next, *h, s, len);
		}

		if (p != NULL) {
			*h = p;
			return (*h)->data;
		}
	}

	/* If we're here, we didn't find a match, we didn't find an inexact
	 * match, or len is 0.  Return s. */
	return (char *)s;
}
