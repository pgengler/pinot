/**************************************************************************
 *   global.c                                                             *
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

#include <list>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include "assert.h"

/* Global variables. */
bool jump_buf_set = false;
sigjmp_buf jump_buf;
/* Used to return to main() after a SIGWINCH. */

Keyboard *keyboard = nullptr;

ssize_t fill = 0;
/* The column where we will wrap lines. */
ssize_t wrap_at = -CHARS_FROM_EOL;
/* The position where we will wrap lines.  fill is equal to this
 * if it's greater than zero, and equal to (COLS + this) if it
 * isn't. */

char *last_search = NULL;
/* The last string we searched for. */
char *last_replace = NULL;
/* The last replacement string we searched for. */

unsigned flags[4] = {0, 0, 0, 0};
/* Our flag containing the states of all global options. */
WINDOW *topwin;
/* The top portion of the window, where we display the version
 * number of pinot, the name of the current file, and whether the
 * current file has been modified. */
WINDOW *edit;
/* The middle portion of the window, i.e. the edit window, where
 * we display the current file we're editing. */
WINDOW *bottomwin;
/* The bottom portion of the window, where we display statusbar
 * messages, the statusbar prompt, and a list of shortcuts. */
int editwinrows = 0;
/* How many rows does the edit window take up? */
int maxrows = 0;
/* How many usable lines are there (due to soft wrapping) */

filestruct *cutbuffer = NULL;
/* The buffer where we store cut text. */
filestruct *cutbottom = NULL;
partition *filepart = NULL;
/* The partition where we store a portion of the current
 * file. */
openfilestruct *openfile = NULL;
/* The list of all open file buffers. */

char *matchbrackets = NULL;
/* The opening and closing brackets that can be found by bracket
 * searches. */

char *whitespace = NULL;
/* The characters used when displaying the first characters of
 * tabs and spaces. */
int whitespace_len[2];
/* The length of these characters. */

bool nodelay_mode = false;
/* Are we in nodelay mode (checking for a cancel wile doing something */

char *answer = NULL;
/* The answer string used by the statusbar prompt. */

ssize_t tabsize = -1;
/* The width of a tab in spaces.  The default value is set in
 * main(). */

char *backup_dir = NULL;
/* The directory where we store backup files. */
const char *locking_prefix = ".";
/* Prefix of how to store the vim-style lock file */
const char *locking_suffix = ".swp";
/* Suffix of the vim-style lock file */

#ifdef ENABLE_SPELLER
char *alt_speller = NULL;
/* The command to use for the alternate spell checker. */
#endif

SyntaxMap syntaxes;
/* The global list of color syntaxes. */
char *syntaxstr = NULL;
/* The color syntax name specified on the command line. */

bool edit_refresh_needed = 0;
/* Did a command mangle enough of the buffer refresh that we
   should repaint the screen */

int currmenu;
/* The currently loaded menu */

std::list<sc*> sclist;
/* New shortcut key struct */
std::list<subnfunc*> allfuncs;
/* New struct for the function list */

filestruct *search_history = NULL;
/* The search string history list. */
filestruct *searchage = NULL;
/* The top of the search string history list. */
filestruct *searchbot = NULL;
/* The bottom of the search string history list. */
filestruct *replace_history = NULL;
/* The replace string history list. */
filestruct *replaceage = NULL;
/* The top of the replace string history list. */
filestruct *replacebot = NULL;
/* The bottom of the replace string history list. */
std::list<poshiststruct *> poshistory;
/* The cursor position history list  */

/* Regular expressions. */
regex_t search_regexp;
/* The compiled regular expression to use in searches. */
regmatch_t regmatches[10];
/* The match positions for parenthetical subexpressions, 10
 * maximum, used in regular expression searches. */

int reverse_attr = A_REVERSE;
/* The curses attribute we use for reverse video. */

char *homedir = NULL;
/* The user's home directory, from $HOME or /etc/passwd. */

/* Return the number of entries in the shortcut list s for a given menu. */
size_t length_of_list(int menu)
{
	size_t i = 0;

	for (auto f : allfuncs) {
		if ((f->menus & menu) != 0 && strlen(f->help) > 0) {
			i++;
		}
	}
	return i;
}

/* Just throw this here */
void case_sens_void(void)
{
}
void regexp_void(void)
{
}
void gototext_void(void)
{
}
void to_files_void(void)
{
}
void dos_format_void(void)
{
}
void mac_format_void(void)
{
}
void append_void(void)
{
}
void prepend_void(void)
{
}
void backup_file_void(void)
{
}
void new_buffer_void(void)
{
}
void backwards_void(void)
{
}
void goto_dir_void(void)
{
}
void no_replace_void(void)
{
}
void ext_cmd_void(void)
{
}

/* Set type of function based on the string */
function_type strtokeytype(const char *str)
{
	if (str[0] ==  'M' || str[0] == 'm') {
		return META;
	} else if (str[0] == '^') {
		return CONTROL;
	} else if (str[0] ==  'F' || str[0] == 'f') {
		return FKEY;
	} else {
		return RAWINPUT;
	}
}

/* Add a string to the new function list strict.
   Does not allow updates, yet anyway */
void add_to_funcs(void (*func)(void), int menus, const char *desc, const char *help, bool blank_after, bool viewok)
{
	auto f = new subnfunc;
	f->scfunc = func;
	f->menus = menus;
	f->desc = desc;
	f->viewok = viewok;
	f->help = help;
	f->blank_after = blank_after;
	allfuncs.push_back(f);

	DEBUG_LOG("Added func \"" << f->desc << '"');
}

const sc *first_sc_for(int menu, void (*func)(void))
{
	const sc *fkeysc = NULL;
	const sc *metasc = NULL;
	const sc *rawsc  = NULL;

	for (auto s : sclist) {
		if ((s->menu & menu) && s->scfunc == func) {
			/* Try to use a meta sequence as as last resorts. Otherwise we will
			   run into problems when we try and handle things like the arrow keys,
			   Home, etc, if for some reason the user bound them to a meta sequence
			   first *shrug* */
			if (s->type == FKEY) {
				if (!fkeysc) {
					fkeysc = s;
				}
				continue;
			} else if (s->type == META) {
				if (!metasc) {
					metasc = s;
				}
				continue;
			} else if (s->type == RAWINPUT) {
				if (!rawsc) {
					rawsc = s;
				}
				continue;
			}
			/* Otherwise it was something else, so use it. */
			return s;
		}
	}

	/* If we're here we may have found only function keys or meta sequences.
	   If so, use one, with the same priority as in the help browser: function
	   keys come first, unless meta sequences are available, in which case meta
	   sequences come first. Last choice is the raw key. */
	if (fkeysc && !metasc) {
		return fkeysc;
	} else if (metasc) {
		return metasc;
	} else if (rawsc) {
		return rawsc;
	}

	DEBUG_LOG("Whoops, returning null given func " << func << " in menu " << menu);
	/* Otherwise... */
	return NULL;
}


/* Add a string to the new shortcut list implementation
   Allows updates to existing entries in the list */
void add_to_sclist(int menu, const char *scstring, void (*func)(void), int toggle, int execute)
{
	auto shortcut = new sc;
	shortcut->type = strtokeytype(scstring);
	shortcut->menu = menu;
	shortcut->toggle = toggle;
	shortcut->keystr = (char *) scstring;
	shortcut->scfunc = func;
	shortcut->execute = execute;
	assign_keyinfo(shortcut);

	DEBUG_LOG("list val = " << shortcut->menu);
	DEBUG_LOG("Setting sequence to " << shortcut->seq << " for shortcut \"" << scstring << '"');

	// First look if we already have a shortcut for this menu and key string
	for (auto s : sclist) {
		if (s->menu == menu && s->keystr == scstring) {
			// If we do, replace the existing one
			*s = *shortcut;
			delete shortcut;
			return;
		}
	}

	sclist.push_back(shortcut);
}

/* Assign one menu's shortcuts to another function */
void replace_scs_for(void (*oldfunc)(void), void (*newfunc)(void))
{
	for (auto s : sclist) {
		if (s->scfunc == oldfunc) {
			s->scfunc = newfunc;
		}
	}
}

void empty_sclist(void)
{
	while (sclist.size() > 0) {
		auto s = sclist.front();
		sclist.pop_front();
		delete s;
	}
}

/* Assign the info to the shortcut struct
   Assumes keystr is already assigned, naturally */
void assign_keyinfo(sc *s)
{
	if (s->type == CONTROL) {
		assert(strlen(s->keystr) > 1);
		s->seq = s->keystr[1] - 64;
	} else if (s->type == META) {
		assert(strlen(s->keystr) > 2);
		s->seq = tolower((int) s->keystr[2]);
	} else if (s->type == FKEY) {
		assert(strlen(s->keystr) > 1);
		s->seq = KEY_F0 + atoi(&s->keystr[1]);
	} else { /* RAWINPUT */
		s->seq = (int) s->keystr[0];
	}

	/* Override some keys which don't bind as nicely as we'd like */
	if (s->type == CONTROL && (!strcasecmp(s->keystr, "Space"))) {
		s->seq = 0;
	} else if (s->type == META && (!strcasecmp(s->keystr, "Space"))) {
		s->seq = (int) ' ';
	} else if (s->type == RAWINPUT && (!strcasecmp(s->keystr, "Up"))) {
		s->seq = KEY_UP;
	} else if (s->type == RAWINPUT && (!strcasecmp(s->keystr, "Down"))) {
		s->seq = KEY_DOWN;
	} else if (s->type == RAWINPUT && (!strcasecmp(s->keystr, "Left"))) {
		s->seq = KEY_LEFT;
	} else if (s->type == RAWINPUT && (!strcasecmp(s->keystr, "Right"))) {
		s->seq = KEY_RIGHT;
	} else if (s->type == RAWINPUT && (!strcasecmp(s->keystr, "Insert"))) {
		s->seq = KEY_IC;
	} else if (s->type == RAWINPUT && (!strcasecmp(s->keystr, "Delete"))) {
		s->seq = KEY_DC;
	} else if (s->type == RAWINPUT && (!strcasecmp(s->keystr, "Backspace"))) {
		s->seq = KEY_BACKSPACE;
	} else if (s->type == RAWINPUT && (!strcasecmp(s->keystr, "Enter"))) {
		s->seq = KEY_ENTER;
	} else if (s->type == RAWINPUT && (!strcasecmp(s->keystr, "PageUp"))) {
		s->seq = KEY_PPAGE;
	} else if (s->type == RAWINPUT && (!strcasecmp(s->keystr, "PageDown"))) {
		s->seq = KEY_NPAGE;
	}
#ifdef KEY_HOME
	else if (s->type == RAWINPUT && (!strcasecmp(s->keystr, "Home"))) {
		s->seq = KEY_HOME;
	}
#endif
#ifdef KEY_END
	else if (s->type == RAWINPUT && (!strcasecmp(s->keystr, "End"))) {
		s->seq = KEY_END;
	}
#endif

}

#ifdef DEBUG

void print_sclist(void)
{
	for (auto s : sclist) {
		auto f = sctofunc(s);
		if (f) {
			DEBUG_LOG("Shortcut \"" << s->keystr << "\", function: " << f->desc << ", menus " << f->menus);
		} else {
			DEBUG_LOG("Hmm, didnt find a func for \"" << s->keystr << '"');
		}
	}
}
#endif


/* TRANSLATORS: Try to keep this to at most 12 characters. */
const char *whereis_next_msg = N_("WhereIs Next");

/* Initialize all shortcut lists. */
void shortcut_init(void)
{
	/* TRANSLATORS: Try to keep the following strings at most 10 characters. */
	const char *exit_msg = N_("Exit");
	const char *whereis_msg = N_("Where Is");
	const char *refresh_msg = N_("Refresh");
	const char *insert_file_msg =  N_("Insert File");
#ifdef ENABLE_SPELLER
	const char *spell_msg = N_("To Spell");
#endif
	const char *lint_msg = N_("To Linter");
	const char *pinot_lint_msg = N_("Invoke the linter, if available");
	/* TRANSLATORS: Try to keep the next two strings to at most 14 characters. */
	const char *prev_lint_msg = N_("Prev Lint Msg");
	const char *next_lint_msg = N_("Next Lint Msg");

	/* TRANSLATORS: The next long series of strings are shortcut descriptions;
	 * they are best kept shorter than 56 characters, but may be longer. */
	const char *pinot_cancel_msg = N_("Cancel the current function");
	const char *pinot_help_msg = N_("Display this help text");
	const char *pinot_exit_msg = N_("Close the current file buffer / Exit from pinot");
	const char *pinot_writeout_msg = N_("Write the current file to disk");
	const char *pinot_insert_msg = N_("Insert another file into the current one");
	const char *pinot_whereis_msg = N_("Search for a string or a regular expression");
	const char *pinot_prevpage_msg = N_("Go to previous screen");
	const char *pinot_nextpage_msg = N_("Go to next screen");
	const char *pinot_cut_msg = N_("Cut the current line and store it in the cutbuffer");
	const char *pinot_uncut_msg = N_("Uncut from the cutbuffer into the current line");
	const char *pinot_cursorpos_msg = N_("Display the position of the cursor");
#ifdef ENABLE_SPELLER
	const char *pinot_spell_msg = N_("Invoke the spell checker, if available");
#endif
	const char *pinot_replace_msg = N_("Replace a string or a regular expression");
	const char *pinot_gotoline_msg = N_("Go to line and column number");
	const char *pinot_mark_msg = N_("Mark text at the cursor position");
	const char *pinot_whereis_next_msg = N_("Repeat last search");
	const char *pinot_copy_msg = N_("Copy the current line and store it in the cutbuffer");
	const char *pinot_indent_msg = N_("Indent the current line");
	const char *pinot_unindent_msg = N_("Unindent the current line");
	const char *pinot_undo_msg = N_("Undo the last operation");
	const char *pinot_redo_msg = N_("Redo the last undone operation");
	const char *pinot_forward_msg = N_("Go forward one character");
	const char *pinot_back_msg = N_("Go back one character");
	const char *pinot_nextword_msg = N_("Go forward one word");
	const char *pinot_prevword_msg = N_("Go back one word");
	const char *pinot_prevline_msg = N_("Go to previous line");
	const char *pinot_nextline_msg = N_("Go to next line");
	const char *pinot_home_msg = N_("Go to beginning of current line");
	const char *pinot_end_msg = N_("Go to end of current line");
	const char *pinot_firstline_msg = N_("Go to the first line of the file");
	const char *pinot_lastline_msg = N_("Go to the last line of the file");
	const char *pinot_bracket_msg = N_("Go to the matching bracket");
	const char *pinot_scrollup_msg = N_("Scroll up one line without scrolling the cursor");
	const char *pinot_scrolldown_msg = N_("Scroll down one line without scrolling the cursor");
	const char *pinot_prevfile_msg = N_("Switch to the previous file buffer");
	const char *pinot_nextfile_msg = N_("Switch to the next file buffer");
	const char *pinot_verbatim_msg = N_("Insert the next keystroke verbatim");
	const char *pinot_tab_msg = N_("Insert a tab at the cursor position");
	const char *pinot_enter_msg = N_("Insert a newline at the cursor position");
	const char *pinot_delete_msg = N_("Delete the character under the cursor");
	const char *pinot_backspace_msg = N_("Delete the character to the left of the cursor");
	const char *pinot_cut_till_end_msg = N_("Cut from the cursor position to the end of the file");
	const char *pinot_wordcount_msg = N_("Count the number of words, lines, and characters");
	const char *pinot_refresh_msg = N_("Refresh (redraw) the current screen");
	const char *pinot_suspend_msg = N_("Suspend the editor (if suspend is enabled)");
	const char *pinot_case_msg = N_("Toggle the case sensitivity of the search");
	const char *pinot_reverse_msg = N_("Reverse the direction of the search");
	const char *pinot_regexp_msg = N_("Toggle the use of regular expressions");
	const char *pinot_prev_history_msg = N_("Recall the previous search/replace string");
	const char *pinot_next_history_msg = N_("Recall the next search/replace string");
	const char *pinot_tofiles_msg = N_("Go to file browser");
	const char *pinot_dos_msg = N_("Toggle the use of DOS format");
	const char *pinot_mac_msg = N_("Toggle the use of Mac format");
	const char *pinot_append_msg = N_("Toggle appending");
	const char *pinot_prepend_msg = N_("Toggle prepending");
	const char *pinot_backup_msg = N_("Toggle backing up of the original file");
	const char *pinot_execute_msg = N_("Execute external command");
	const char *pinot_multibuffer_msg = N_("Toggle the use of a new buffer");
	const char *pinot_exitbrowser_msg = N_("Exit from the file browser");
	const char *pinot_firstfile_msg = N_("Go to the first file in the list");
	const char *pinot_lastfile_msg = N_("Go to the last file in the list");
	const char *pinot_forwardfile_msg = N_("Go to the next file in the list");
	const char *pinot_backfile_msg = N_("Go to the previous file in the list");
	const char *pinot_gotodir_msg = N_("Go to directory");
	const char *pinot_prevlint_msg = N_("Go to previous linter msg");
	const char *pinot_nextlint_msg = N_("Go to next linter msg");

// FIXME
#define IFSCHELP(help) help

	while (allfuncs.size() > 0) {
		auto f = allfuncs.front();
		allfuncs.pop_front();
		delete f;
	}

	/* TRANSLATORS: Try to keep the "Get Help" string to at most 10 characters. */
	add_to_funcs(do_help_void, MMOST, N_("Get Help"), IFSCHELP(pinot_help_msg), false, VIEW);

	add_to_funcs(do_cancel,
	              (MWHEREIS|MREPLACE|MREPLACEWITH|MGOTOLINE|MWRITEFILE|MINSERTFILE|MEXTCMD|MSPELL|MWHEREISFILE|MGOTODIR|MYESNO|MLINTER),
	              N_("Cancel"), IFSCHELP(pinot_cancel_msg), false, VIEW);

	add_to_funcs(do_exit, MMAIN, openfile != NULL && openfile != openfile->next ? N_("Close") : exit_msg, IFSCHELP(pinot_exit_msg), false, VIEW);

	add_to_funcs(do_exit, MBROWSER, exit_msg, IFSCHELP(pinot_exitbrowser_msg), false, VIEW);

	add_to_funcs(do_writeout_void, MMAIN, N_("WriteOut"), IFSCHELP(pinot_writeout_msg), false, NOVIEW);

	add_to_funcs(do_insertfile_void, MMAIN, N_("Read File"), IFSCHELP(pinot_insert_msg), false, VIEW);

	add_to_funcs(do_search, MMAIN|MBROWSER, whereis_msg, IFSCHELP(pinot_whereis_msg), false, VIEW);

	add_to_funcs(do_page_up, MMAIN|MHELP|MBROWSER, N_("Prev Page"), IFSCHELP(pinot_prevpage_msg), false, VIEW);
	add_to_funcs(do_page_down, MMAIN|MHELP|MBROWSER, N_("Next Page"), IFSCHELP(pinot_nextpage_msg), true, VIEW);

	add_to_funcs(do_page_up, MLINTER, prev_lint_msg, IFSCHELP(pinot_prevlint_msg), false, VIEW);
	add_to_funcs(do_page_down, MLINTER, next_lint_msg, IFSCHELP(pinot_nextlint_msg), false, VIEW);

	add_to_funcs(do_cut_text_void, MMAIN, N_("Cut Text"), IFSCHELP(pinot_cut_msg), false, NOVIEW);

	add_to_funcs(do_uncut_text, MMAIN, N_("UnCut Text"), IFSCHELP(pinot_uncut_msg), false, NOVIEW);

	add_to_funcs(do_cursorpos_void, MMAIN, N_("Cur Pos"), IFSCHELP(pinot_cursorpos_msg), false, VIEW);

#ifdef ENABLE_SPELLER
	/* TRANSLATORS: Try to keep this at most 10 characters. */
	add_to_funcs(do_spell, MMAIN, spell_msg, IFSCHELP(pinot_spell_msg), true, NOVIEW);
#endif
	add_to_funcs(do_linter, MMAIN, lint_msg, IFSCHELP(pinot_lint_msg), true, NOVIEW);

	add_to_funcs(do_first_line, (MMAIN|MHELP|MWHEREIS|MREPLACE|MREPLACEWITH|MGOTOLINE), N_("First Line"), IFSCHELP(pinot_firstline_msg), false, VIEW);
	add_to_funcs(do_last_line, (MMAIN|MHELP|MWHEREIS|MREPLACE|MREPLACEWITH|MGOTOLINE), N_("Last Line"), IFSCHELP(pinot_lastline_msg), true, VIEW);

	add_to_funcs(do_gotolinecolumn_void, (MMAIN|MWHEREIS), N_("Go To Line"), IFSCHELP(pinot_gotoline_msg), false, VIEW);

	/* TRANSLATORS: Try to keep this at most 10 characters. */
	add_to_funcs(do_cursorpos_void, MMAIN, N_("Cur Pos"), IFSCHELP(pinot_cursorpos_msg), false, VIEW);

	add_to_funcs(do_replace, (MMAIN|MWHEREIS), _("Replace"), IFSCHELP(pinot_replace_msg), false, NOVIEW);

	add_to_funcs(do_mark, MMAIN, N_("Mark Text"), IFSCHELP(pinot_mark_msg), false, VIEW);

	add_to_funcs(do_research, (MMAIN|MBROWSER), whereis_next_msg, IFSCHELP(pinot_whereis_next_msg), true, VIEW);

	add_to_funcs(do_copy_text, MMAIN, N_("Copy Text"), IFSCHELP(pinot_copy_msg), false, NOVIEW);

	add_to_funcs(do_indent_void, MMAIN, N_("Indent Text"), IFSCHELP(pinot_indent_msg), false, NOVIEW);

	add_to_funcs(do_unindent, MMAIN, N_("Unindent Text"), IFSCHELP(pinot_unindent_msg), false, NOVIEW);

	if (ISSET(UNDOABLE)) {
		add_to_funcs(do_undo, MMAIN, N_("Undo"), IFSCHELP(pinot_undo_msg), false, NOVIEW);
		add_to_funcs(do_redo, MMAIN, N_("Redo"), IFSCHELP(pinot_redo_msg), true, NOVIEW);
	}

	add_to_funcs(do_execute_command, MMAIN, N_("Exec Cmd"), IFSCHELP(pinot_execute_msg), false, NOVIEW);

	add_to_funcs(do_right, MMAIN, N_("Forward"), IFSCHELP(pinot_forward_msg), false, VIEW);

	add_to_funcs(do_right, MBROWSER, N_("Forward"), IFSCHELP(pinot_forwardfile_msg), false, VIEW);

	add_to_funcs(do_left, MMAIN, N_("Back"), IFSCHELP(pinot_back_msg), false, VIEW);

	add_to_funcs(do_left, MBROWSER, N_("Back"), IFSCHELP(pinot_backfile_msg), false, VIEW);

	add_to_funcs(do_prev_word_void, MMAIN, N_("Prev Word"), IFSCHELP(pinot_prevword_msg), false, VIEW);

	add_to_funcs(do_next_word_void, MMAIN, N_("Next Word"), IFSCHELP(pinot_nextword_msg), false, VIEW);

	add_to_funcs(do_up_void, (MMAIN|MHELP|MBROWSER), N_("Prev Line"), IFSCHELP(pinot_prevline_msg), false, VIEW);

	add_to_funcs(do_down_void, (MMAIN|MHELP|MBROWSER), N_("Next Line"), IFSCHELP(pinot_nextline_msg), true, VIEW);

	add_to_funcs(do_home, MMAIN, N_("Home"), IFSCHELP(pinot_home_msg), false, VIEW);

	add_to_funcs(do_end, MMAIN, N_("End"), IFSCHELP(pinot_end_msg), false, VIEW);

	add_to_funcs(do_find_bracket, MMAIN, _("Find Other Bracket"), IFSCHELP(pinot_bracket_msg), false, VIEW);

	add_to_funcs(do_scroll_up, MMAIN, N_("Scroll Up"), IFSCHELP(pinot_scrollup_msg), false, VIEW);

	add_to_funcs(do_scroll_down, MMAIN, N_("Scroll Down"), IFSCHELP(pinot_scrolldown_msg), false, VIEW);

	add_to_funcs(switch_to_prev_buffer_void, MMAIN, _("Previous File"), IFSCHELP(pinot_prevfile_msg), false, VIEW);
	add_to_funcs(switch_to_next_buffer_void, MMAIN, N_("Next File"), IFSCHELP(pinot_nextfile_msg), true, VIEW);

	add_to_funcs(do_verbatim_input, MMAIN, N_("Verbatim Input"), IFSCHELP(pinot_verbatim_msg), false, NOVIEW);

	add_to_funcs(do_tab, MMAIN, N_("Tab"), IFSCHELP(pinot_tab_msg), false, NOVIEW);
	add_to_funcs(do_enter_void, MMAIN, N_("Enter"), IFSCHELP(pinot_enter_msg), false, NOVIEW);
	add_to_funcs(do_delete, MMAIN, N_("Delete"), IFSCHELP(pinot_delete_msg), false, NOVIEW);
	add_to_funcs(do_backspace, MMAIN, N_("Backspace"), IFSCHELP(pinot_backspace_msg), false, NOVIEW);

	add_to_funcs(do_cut_till_end, MMAIN, N_("CutTillEnd"), IFSCHELP(pinot_cut_till_end_msg), true, NOVIEW);

	add_to_funcs(do_wordlinechar_count, MMAIN, N_("Word Count"), IFSCHELP(pinot_wordcount_msg), false, VIEW);

	add_to_funcs(total_refresh, (MMAIN|MHELP), refresh_msg, IFSCHELP(pinot_refresh_msg), false, VIEW);

	add_to_funcs(do_suspend_void, MMAIN, N_("Suspend"), IFSCHELP(pinot_suspend_msg), true, VIEW);

	add_to_funcs(case_sens_void, (MWHEREIS|MREPLACE|MWHEREISFILE), N_("Case Sens"), IFSCHELP(pinot_case_msg), false, VIEW);

	add_to_funcs(backwards_void, (MWHEREIS|MREPLACE|MWHEREISFILE), N_("Backwards"), IFSCHELP(pinot_reverse_msg), false, VIEW);

	add_to_funcs(regexp_void, (MWHEREIS|MREPLACE|MWHEREISFILE), N_("Regexp"), IFSCHELP(pinot_regexp_msg), false, VIEW);

	add_to_funcs(get_history_older_void, (MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE), N_("PrevHist"), IFSCHELP(pinot_prev_history_msg), false, VIEW);

	add_to_funcs(get_history_newer_void, (MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE), N_("NextHist"), IFSCHELP(pinot_next_history_msg), false, VIEW);

	add_to_funcs(no_replace_void, MREPLACE, N_("No Replace"), IFSCHELP(pinot_whereis_msg), false, VIEW);

	add_to_funcs(gototext_void, MGOTOLINE, N_("Go To Text"), IFSCHELP(pinot_whereis_msg), true, VIEW);

	add_to_funcs(to_files_void, MINSERTFILE, N_("To Files"), IFSCHELP(pinot_tofiles_msg), false, VIEW);

	add_to_funcs(dos_format_void, MWRITEFILE, N_("DOS Format"), IFSCHELP(pinot_dos_msg), false, NOVIEW);
	add_to_funcs(mac_format_void, MWRITEFILE, N_("Mac Format"), IFSCHELP(pinot_mac_msg), false, NOVIEW);
	add_to_funcs(append_void, MWRITEFILE, N_("Append"), IFSCHELP(pinot_append_msg), false, NOVIEW);
	add_to_funcs(prepend_void, MWRITEFILE, N_("Prepend"), IFSCHELP(pinot_prepend_msg), false, NOVIEW);
	add_to_funcs( backup_file_void, MWRITEFILE, N_("Backup File"), IFSCHELP(pinot_backup_msg), false, NOVIEW);

	add_to_funcs(ext_cmd_void, MINSERTFILE, N_("Execute Command"), IFSCHELP(pinot_execute_msg), false, NOVIEW);

	add_to_funcs(new_buffer_void, MINSERTFILE|MEXTCMD, N_("New Buffer"), IFSCHELP(pinot_multibuffer_msg), false, NOVIEW);

	add_to_funcs(do_insertfile_void, MEXTCMD, insert_file_msg, IFSCHELP(pinot_insert_msg), false, VIEW);

	add_to_funcs(edit_refresh, MHELP, refresh_msg, pinot_refresh_msg, false, VIEW);

	add_to_funcs(do_exit, MHELP, exit_msg, IFSCHELP(pinot_exit_msg), false, VIEW);

	add_to_funcs(do_first_file, (MBROWSER|MWHEREISFILE), N_("First File"), IFSCHELP(pinot_firstfile_msg), false, VIEW);

	add_to_funcs(do_last_file, (MBROWSER|MWHEREISFILE), N_("Last File"), IFSCHELP(pinot_lastfile_msg), false, VIEW);

	add_to_funcs(goto_dir_void, MBROWSER, N_("Go To Dir"), IFSCHELP(pinot_gotodir_msg), false, VIEW);

	empty_sclist();

	add_to_sclist(MMOST, "^G", do_help_void, 0, true);
	add_to_sclist(MMOST, "F1", do_help_void, 0, true);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "^X", do_exit, 0, true);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "F2", do_exit, 0, true);
	add_to_sclist(MMAIN, "^_", do_gotolinecolumn_void, 0, true);
	add_to_sclist(MMAIN, "F13", do_gotolinecolumn_void, 0, true);
	add_to_sclist(MMAIN, "M-G", do_gotolinecolumn_void, 0, true);
	add_to_sclist(MMAIN, "^O", do_writeout_void, 0, true);
	add_to_sclist(MMAIN, "F3", do_writeout_void, 0, true);
	add_to_sclist(MMAIN, "^R", do_insertfile_void, 0, true);
	add_to_sclist(MMAIN, "F5", do_insertfile_void, 0, true);
	add_to_sclist(MMAIN, "Insert", do_insertfile_void, 0, true);
	add_to_sclist(MMAIN|MBROWSER, "^W", do_search, 0, true);
	add_to_sclist(MMAIN|MBROWSER, "F6", do_search, 0, true);
	add_to_sclist(MMAIN|MBROWSER|MHELP|MWHEREISFILE|MLINTER, "^Y", do_page_up, 0, true);
	add_to_sclist(MMAIN|MBROWSER|MHELP|MWHEREISFILE|MLINTER, "F7", do_page_up, 0, true);
	add_to_sclist(MMAIN|MBROWSER|MHELP|MWHEREISFILE|MLINTER, "PageUp", do_page_up, 0, true);
	add_to_sclist(MMAIN|MBROWSER|MHELP|MWHEREISFILE|MLINTER, "^V", do_page_down, 0, true);
	add_to_sclist(MMAIN|MBROWSER|MHELP|MWHEREISFILE|MLINTER, "F8", do_page_down, 0, true);
	add_to_sclist(MMAIN|MBROWSER|MHELP|MWHEREISFILE|MLINTER, "PageDown", do_page_down, 0, true);
	add_to_sclist(MMAIN, "^K", do_cut_text_void, 0, true);
	add_to_sclist(MMAIN, "F9", do_cut_text_void, 0, true);
	add_to_sclist(MMAIN, "^U", do_uncut_text, 0, true);
	add_to_sclist(MMAIN, "F10", do_uncut_text, 0, true);
	add_to_sclist(MMAIN, "^C", do_cursorpos_void, 0, true);
	add_to_sclist(MMAIN, "F11", do_cursorpos_void, 0, true);
#ifdef ENABLE_SPELLER
	add_to_sclist(MMAIN, "^T", do_spell, 0, true);
	add_to_sclist(MMAIN, "F12", do_spell, 0, true);
#endif
	add_to_sclist(MMAIN, "^\\", do_replace, 0, true);
	add_to_sclist(MMAIN, "F14", do_replace, 0, true);
	add_to_sclist(MMAIN, "M-R", do_replace, 0, true);
	add_to_sclist(MWHEREIS, "^R", do_replace, 0, false);
	add_to_sclist(MREPLACE, "^R", no_replace_void, 0, false);
	add_to_sclist(MWHEREIS, "^T", do_gotolinecolumn_void, 0, false);
	add_to_sclist(MMAIN, "^^", do_mark, 0, true);
	add_to_sclist(MMAIN, "F15", do_mark, 0, true);
	add_to_sclist(MMAIN, "M-A", do_mark, 0, true);
	add_to_sclist(MMAIN|MBROWSER, "M-W", do_research, 0, true);
	add_to_sclist(MMAIN|MBROWSER, "F16", do_research, 0, true);
	add_to_sclist(MMAIN, "M-^", do_copy_text, 0, true);
	add_to_sclist(MMAIN, "M-6", do_copy_text, 0, true);
	add_to_sclist(MMAIN, "M-}", do_indent_void, 0, true);
	add_to_sclist(MMAIN, "M-{", do_unindent, 0, true);
	if (ISSET(UNDOABLE)) {
		add_to_sclist(MMAIN, "M-U", do_undo, 0, true);
		add_to_sclist(MMAIN, "M-E", do_redo, 0, true);
	}
	add_to_sclist(MMOST, "^F", do_right, 0, true);
	add_to_sclist(MMOST, "Right", do_right, 0, true);
	add_to_sclist(MMOST, "^B", do_left, 0, true);
	add_to_sclist(MMOST, "Left", do_left, 0, true);

	add_to_sclist(MMAIN, "^@", do_next_word_void, 0, true);
	add_to_sclist(MMAIN, "M- ", do_prev_word_void, 0, true);

	add_to_sclist(MMAIN, "^Q", xon_complaint, 0, true);
	add_to_sclist(MMAIN, "^S", xoff_complaint, 0, true);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "^P", do_up_void, 0, true);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "Up", do_up_void, 0, true);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "^N", do_down_void, 0, true);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "Down", do_down_void, 0, true);
	add_to_sclist(MMOST, "^A", do_home, 0, true);
	add_to_sclist(MMOST, "Home", do_home, 0, true);
	add_to_sclist(MMOST, "^E", do_end, 0, true);
	add_to_sclist(MMOST, "End", do_end, 0, true);
	add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE, "^P", get_history_older_void, 0, false);
	add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE, "Up", get_history_older_void, 0, false);
	add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE, "^N", get_history_newer_void, 0, false);
	add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE, "Down", get_history_newer_void, 0, false);
	add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH, "M-C", case_sens_void, 0, false);
	add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH, "M-B", backwards_void, 0, false);
	add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH, "M-R", regexp_void, 0, false);

	add_to_sclist(MMAIN|MHELP, "M-\\", do_first_line, 0, true);
	add_to_sclist(MMAIN|MHELP, "M-|", do_first_line, 0, true);
	add_to_sclist(MMAIN|MHELP, "M-/", do_last_line, 0, true);
	add_to_sclist(MMAIN|MHELP, "M-?", do_last_line, 0, true);
	add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MGOTOLINE|MHELP, "^Y", do_first_line, 0, true);
	add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MGOTOLINE|MHELP, "^V", do_last_line, 0, true);

	add_to_sclist(MBROWSER|MWHEREISFILE, "M-\\", do_first_file, 0, true);
	add_to_sclist(MBROWSER|MWHEREISFILE, "M-|", do_first_file, 0, true);
	add_to_sclist(MBROWSER|MWHEREISFILE, "M-/", do_last_file, 0, true);
	add_to_sclist(MBROWSER|MWHEREISFILE, "M-?", do_last_file, 0, true);
	add_to_sclist(MBROWSER|MWHEREISFILE, "^_", goto_dir_void, 0, true);
	add_to_sclist(MBROWSER|MWHEREISFILE, "F13", goto_dir_void, 0, true);
	add_to_sclist(MBROWSER|MWHEREISFILE, "M-G", goto_dir_void, 0, true);
	add_to_sclist(MMAIN, "M-]", do_find_bracket, 0, true);
	add_to_sclist(MMAIN, "M--", do_scroll_up, 0, true);
	add_to_sclist(MMAIN, "M-_", do_scroll_up, 0, true);
	add_to_sclist(MMAIN, "M-+", do_scroll_down, 0, true);
	add_to_sclist(MMAIN, "M-=", do_scroll_down, 0, true);

	add_to_sclist(MMAIN, "M-<", switch_to_prev_buffer_void, 0, true);
	add_to_sclist(MMAIN, "M-,", switch_to_prev_buffer_void, 0, true);
	add_to_sclist(MMAIN, "M->", switch_to_next_buffer_void, 0, true);
	add_to_sclist(MMAIN, "M-.", switch_to_next_buffer_void, 0, true);
	add_to_sclist(MMOST, "M-V", do_verbatim_input, 0, true);
	add_to_sclist(MMAIN, "M-T", do_cut_till_end, 0, true);
	add_to_sclist(MMAIN, "M-D", do_wordlinechar_count, 0, true);
	add_to_sclist(MMAIN, "M-X", do_toggle_void, NO_HELP, true);
	add_to_sclist(MMAIN, "M-C", do_toggle_void, CONST_UPDATE, true);
	add_to_sclist(MMAIN, "M-O", do_toggle_void, MORE_SPACE, true);
	add_to_sclist(MMAIN, "M-S", do_toggle_void, SMOOTH_SCROLL, true);
	add_to_sclist(MMAIN, "M-P", do_toggle_void, WHITESPACE_DISPLAY, true);
	add_to_sclist(MMAIN, "M-Y", do_toggle_void, NO_COLOR_SYNTAX, true);
	add_to_sclist(MMAIN, "M-H", do_toggle_void, SMART_HOME, true);
	add_to_sclist(MMAIN, "M-I", do_toggle_void, AUTOINDENT, true);
	add_to_sclist(MMAIN, "M-K", do_toggle_void, CUT_TO_END, true);
	add_to_sclist(MMAIN, "M-L", do_toggle_void, NO_WRAP, true);
	add_to_sclist(MMAIN, "M-Q", do_toggle_void, TABS_TO_SPACES, true);
	add_to_sclist(MMAIN, "M-B", do_toggle_void, BACKUP_FILE, true);
	add_to_sclist(MMAIN, "M-F", do_toggle_void, MULTIBUFFER, true);
	add_to_sclist(MMAIN, "M-N", do_toggle_void, NO_CONVERT, true);
	add_to_sclist(MMAIN, "M-Z", do_toggle_void, SUSPEND, true);
	add_to_sclist(MMAIN, "M-$", do_toggle_void, SOFTWRAP, true);
	add_to_sclist(MHELP|MBROWSER, "^C", do_exit, 0, true);
	add_to_sclist(MHELP, "^G", do_exit, 0, true);
	add_to_sclist(MGOTOLINE, "^T",  gototext_void, 0, false);
	add_to_sclist(MINSERTFILE|MEXTCMD, "M-F", new_buffer_void, 0, false);
	add_to_sclist((MWHEREIS|MREPLACE|MREPLACEWITH|MGOTOLINE|MWRITEFILE|MINSERTFILE|MEXTCMD|MSPELL|MWHEREISFILE|MGOTODIR|MYESNO|MLINTER), "^C", do_cancel, 0, false);
	add_to_sclist(MWRITEFILE, "M-D", dos_format_void, 0, false);
	add_to_sclist(MWRITEFILE, "M-M", mac_format_void, 0, false);
	add_to_sclist(MWRITEFILE, "M-A", append_void, 0, false);
	add_to_sclist(MWRITEFILE, "M-P", prepend_void, 0, false);
	add_to_sclist(MWRITEFILE, "M-B", backup_file_void, 0, false);
	add_to_sclist(MWRITEFILE|MINSERTFILE, "^T", to_files_void, 0, false);
	add_to_sclist(MINSERTFILE, "^X", ext_cmd_void, 0, false);
	add_to_sclist(MMAIN, "^Z", do_suspend_void, 0, false);
	add_to_sclist(MMAIN|MHELP, "^L", total_refresh, 0, true);
	add_to_sclist(MMOST, "Tab", do_tab, 0, true);
	add_to_sclist(MMOST, "Enter", do_enter_void, 0, true);
	add_to_sclist(MMOST, "KPEnter", do_enter_void, 0, true);
	add_to_sclist(MMOST, "^D", do_delete, 0, true);
	add_to_sclist(MMOST, "Delete", do_delete, 0, true);
	add_to_sclist(MMOST, "Backspace", do_backspace, 0, true);

#ifdef DEBUG
	print_sclist();
#endif

}

void set_lint_shortcuts(void)
{
#ifdef ENABLE_SPELLER
	replace_scs_for(do_spell, do_linter);
#endif
}

void set_spell_shortcuts(void)
{
#ifdef ENABLE_SPELLER
	replace_scs_for(do_linter, do_spell);
#endif
}

const subnfunc *sctofunc(sc *s)
{
	for (auto f : allfuncs) {
		if (s->scfunc == f->scfunc) {
			return f;
		}
	}
	return nullptr;
}

/* Now lets come up with a single (hopefully)
   function to get a string for each flag */
const char *flagtostr(int flag)
{
	/* TRANSLATORS: The next seventeen strings are toggle descriptions;
	 * they are best kept shorter than 40 characters, but may be longer. */
	switch (flag) {
	case NO_HELP:
		return N_("Help mode");
	case CONST_UPDATE:
		return N_("Constant cursor position display");
	case MORE_SPACE:
		return N_("Use of one more line for editing");
	case SMOOTH_SCROLL:
		return N_("Smooth scrolling");
	case WHITESPACE_DISPLAY:
		return N_("Whitespace display");
	case NO_COLOR_SYNTAX:
		return N_("Color syntax highlighting");
	case SMART_HOME:
		return N_("Smart home key");
	case AUTOINDENT:
		return N_("Auto indent");
	case CUT_TO_END:
		return N_("Cut to end");
	case NO_WRAP:
		return N_("Long line wrapping");
	case TABS_TO_SPACES:
		return N_("Conversion of typed tabs to spaces");
	case BACKUP_FILE:
		return N_("Backup files");
	case MULTIBUFFER:
		return N_("Multiple file buffers");
	case NO_CONVERT:
		return N_("No conversion from DOS/Mac format");
	case SUSPEND:
		return N_("Suspension");
	case SOFTWRAP:
		return N_("Soft line wrapping");
	default:
		return "?????";
	}
}

/* Interpret the string given by the rc file and return a
    shortcut struct, complete with proper value for execute */
sc *strtosc(char *input)
{
	sc *s;

	s = new sc;
	s->execute = true; /* overridden as needed below */


	if (!strcasecmp(input, "help")) {
		s->scfunc = do_help_void;
	} else {
		if (!strcasecmp(input, "cancel")) {
			s->scfunc = do_cancel;
			s->execute = false;
		} else if (!strcasecmp(input, "exit")) {
			s->scfunc = do_exit;
		} else if (!strcasecmp(input, "writeout")) {
			s->scfunc = do_writeout_void;
		} else if (!strcasecmp(input, "insert")) {
			s->scfunc = do_insertfile_void;
		} else if (!strcasecmp(input, "execute")) {
			s->scfunc = do_execute_command;
		} else if (!strcasecmp(input, "whereis")) {
			s->scfunc = do_search;
		} else if (!strcasecmp(input, "up")) {
			s->scfunc = do_up_void;
		} else if (!strcasecmp(input, "down")) {
			s->scfunc = do_down_void;
		} else if (!strcasecmp(input, "pageup") || !strcasecmp(input, "prevpage")) {
			s->scfunc = do_page_up;
		} else if (!strcasecmp(input, "pagedown") || !strcasecmp(input, "nextpage")) {
			s->scfunc = do_page_down;
		} else if (!strcasecmp(input, "cut")) {
			s->scfunc = do_cut_text_void;
		} else if (!strcasecmp(input, "uncut")) {
			s->scfunc = do_uncut_text;
		} else if (!strcasecmp(input, "curpos") || !strcasecmp(input, "cursorpos")) {
			s->scfunc = do_cursorpos_void;
		} else if (!strcasecmp(input, "firstline")) {
			s->scfunc = do_first_line;
		} else if (!strcasecmp(input, "lastline")) {
			s->scfunc = do_last_line;
		} else if (!strcasecmp(input, "gotoline")) {
			s->scfunc = do_gotolinecolumn_void;
		} else if (!strcasecmp(input, "replace")) {
			s->scfunc = do_replace;
		} else if (!strcasecmp(input, "mark")) {
			s->scfunc = do_mark;
		} else if (!strcasecmp(input, "searchagain") || !strcasecmp(input, "research")) {
			s->scfunc = do_research;
		} else if (!strcasecmp(input, "copytext")) {
			s->scfunc = do_copy_text;
		} else if (!strcasecmp(input, "indent")) {
			s->scfunc = do_indent_void;
		} else if (!strcasecmp(input, "unindent")) {
			s->scfunc = do_unindent;
		} else if (!strcasecmp(input, "scrollup")) {
			s->scfunc = do_scroll_up;
		} else if (!strcasecmp(input, "scrolldown")) {
			s->scfunc = do_scroll_down;
		} else if (!strcasecmp(input, "nextword")) {
			s->scfunc = do_next_word_void;
		} else if (!strcasecmp(input, "suspend")) {
			s->scfunc = do_suspend_void;
		} else if (!strcasecmp(input, "prevword")) {
			s->scfunc = do_prev_word_void;
		} else if (!strcasecmp(input, "findbracket")) {
			s->scfunc = do_find_bracket;
		} else if (!strcasecmp(input, "wordcount")) {
			s->scfunc = do_wordlinechar_count;
		} else if (!strcasecmp(input, "undo")) {
			s->scfunc = do_undo;
		} else if (!strcasecmp(input, "redo")) {
			s->scfunc = do_redo;
		} else if (!strcasecmp(input, "prevhistory")) {
			s->scfunc =  get_history_older_void;
			s->execute = false;
		} else if (!strcasecmp(input, "nexthistory")) {
			s->scfunc =  get_history_newer_void;
			s->execute = false;
		} else if (!strcasecmp(input, "nohelp")) {
			s->scfunc =  do_toggle_void;
			s->execute = false;
			s->toggle = NO_HELP;
		} else if (!strcasecmp(input, "constupdate")) {
			s->scfunc =  do_toggle_void;
			s->execute = false;
			s->toggle = CONST_UPDATE;
		} else if (!strcasecmp(input, "morespace")) {
			s->scfunc =  do_toggle_void;
			s->execute = false;
			s->toggle = MORE_SPACE;
		} else if (!strcasecmp(input, "smoothscroll")) {
			s->scfunc =  do_toggle_void;
			s->execute = false;
			s->toggle = SMOOTH_SCROLL;
		} else if (!strcasecmp(input, "whitespacedisplay")) {
			s->scfunc =  do_toggle_void;
			s->execute = false;
			s->toggle = WHITESPACE_DISPLAY;
		} else if (!strcasecmp(input, "nosyntax")) {
			s->scfunc =  do_toggle_void;
			s->execute = false;
			s->toggle = NO_COLOR_SYNTAX;
		} else if (!strcasecmp(input, "smarthome")) {
			s->scfunc =  do_toggle_void;
			s->execute = false;
			s->toggle = SMART_HOME;
		} else if (!strcasecmp(input, "autoindent")) {
			s->scfunc =  do_toggle_void;
			s->execute = false;
			s->toggle = AUTOINDENT;
		} else if (!strcasecmp(input, "cuttoend")) {
			s->scfunc =  do_toggle_void;
			s->execute = false;
			s->toggle = CUT_TO_END;
		} else if (!strcasecmp(input, "nowrap")) {
			s->scfunc =  do_toggle_void;
			s->execute = false;
			s->toggle = NO_WRAP;
		} else if (!strcasecmp(input, "softwrap")) {
			s->scfunc =  do_toggle_void;
			s->execute = false;
			s->toggle = SOFTWRAP;
		} else if (!strcasecmp(input, "tabstospaces")) {
			s->scfunc =  do_toggle_void;
			s->execute = false;
			s->toggle = TABS_TO_SPACES;
		} else if (!strcasecmp(input, "backupfile")) {
			s->scfunc =  do_toggle_void;
			s->execute = false;
			s->toggle = BACKUP_FILE;
		} else if (!strcasecmp(input, "multibuffer")) {
			s->scfunc =  do_toggle_void;
			s->execute = false;
			s->toggle = MULTIBUFFER;
		} else if (!strcasecmp(input, "noconvert")) {
			s->scfunc =  do_toggle_void;
			s->execute = false;
			s->toggle = NO_CONVERT;
		} else if (!strcasecmp(input, "suspendenable")) {
			s->scfunc =  do_toggle_void;
			s->execute = false;
			s->toggle = SUSPEND;
		} else if (!strcasecmp(input, "right") || !strcasecmp(input, "forward")) {
			s->scfunc = do_right;
		} else if (!strcasecmp(input, "left") || !strcasecmp(input, "back")) {
			s->scfunc = do_left;
		} else if (!strcasecmp(input, "up") || !strcasecmp(input, "prevline")) {
			s->scfunc = do_up_void;
		} else if (!strcasecmp(input, "down") || !strcasecmp(input, "nextline")) {
			s->scfunc = do_down_void;
		} else if (!strcasecmp(input, "home")) {
			s->scfunc = do_home;
		} else if (!strcasecmp(input, "end")) {
			s->scfunc = do_end;
		} else if (!strcasecmp(input, "prevbuf")) {
			s->scfunc = switch_to_prev_buffer_void;
		} else if (!strcasecmp(input, "nextbuf")) {
			s->scfunc = switch_to_next_buffer_void;
		} else if (!strcasecmp(input, "verbatim")) {
			s->scfunc = do_verbatim_input;
		} else if (!strcasecmp(input, "tab")) {
			s->scfunc = do_tab;
		} else if (!strcasecmp(input, "enter")) {
			s->scfunc = do_enter_void;
		} else if (!strcasecmp(input, "delete")) {
			s->scfunc = do_delete;
		} else if (!strcasecmp(input, "backspace")) {
			s->scfunc = do_backspace;
		} else if (!strcasecmp(input, "refresh")) {
			s->scfunc = total_refresh;
		} else if (!strcasecmp(input, "casesens")) {
			s->scfunc = case_sens_void;
			s->execute = false;
		} else if (!strcasecmp(input, "regexp") || !strcasecmp(input, "regex")) {
			s->scfunc = regexp_void;
			s->execute = false;
		} else if (!strcasecmp(input, "dontreplace")) {
			s->scfunc = no_replace_void;
			s->execute = false;
		} else if (!strcasecmp(input, "gototext")) {
			s->scfunc = gototext_void;
			s->execute = false;
		} else if (!strcasecmp(input, "browser") || !strcasecmp(input, "tofiles")) {
			s->scfunc = to_files_void;
			s->execute = false;
		} else if (!strcasecmp(input, "dosformat")) {
			s->scfunc = dos_format_void;
			s->execute = false;
		} else if (!strcasecmp(input, "macformat")) {
			s->scfunc =  mac_format_void;
			s->execute = false;
		} else if (!strcasecmp(input, "append")) {
			s->scfunc =  append_void;
			s->execute = false;
		} else if (!strcasecmp(input, "prepend")) {
			s->scfunc =  prepend_void;
			s->execute = false;
		} else if (!strcasecmp(input, "backup")) {
			s->scfunc =  backup_file_void;
			s->execute = false;
		} else if (!strcasecmp(input, "newbuffer")) {
			s->scfunc =  new_buffer_void;
			s->execute = false;
		} else if (!strcasecmp(input, "firstfile")) {
			s->scfunc =  do_first_file;
			s->execute = false;
		} else if (!strcasecmp(input, "lastfile")) {
			s->scfunc = do_last_file;
			s->execute = false;
		} else {
			delete s;
			return NULL;
		}
	}

	return s;

}

/* Same thing as abnove but for the menu */
int strtomenu(char *input)
{
	if (!strcasecmp(input, "all")) {
		return MMOST;
	} else if (!strcasecmp(input, "main")) {
		return MMAIN;
	} else if (!strcasecmp(input, "search")) {
		return MWHEREIS;
	} else if (!strcasecmp(input, "replace")) {
		return MREPLACE;
	} else if (!strcasecmp(input, "replace2") || !strcasecmp(input, "replacewith")) {
		return MREPLACEWITH;
	} else if (!strcasecmp(input, "gotoline")) {
		return MGOTOLINE;
	} else if (!strcasecmp(input, "writeout")) {
		return MWRITEFILE;
	} else if (!strcasecmp(input, "insert")) {
		return MINSERTFILE;
	} else if (!strcasecmp(input, "externalcmd") || !strcasecmp(input, "extcmd")) {
		return MEXTCMD;
	} else if (!strcasecmp(input, "help")) {
		return MHELP;
	} else if (!strcasecmp(input, "spell")) {
		return MSPELL;
	} else if (!strcasecmp(input, "browser")) {
		return MBROWSER;
	} else if (!strcasecmp(input, "whereisfile")) {
		return MWHEREISFILE;
	} else if (!strcasecmp(input, "gotodir")) {
		return MGOTODIR;
	}

	return -1;
}


#ifdef DEBUG
/* This function is used to gracefully return all the memory we've used.
 * It should be called just before calling exit().  Practically, the
 * only effect is to cause a segmentation fault if the various data
 * structures got bolloxed earlier.  Thus, we don't bother having this
 * function unless debugging is turned on. */
void thanks_for_all_the_fish(void)
{
	delwin(topwin);
	delwin(edit);
	delwin(bottomwin);

	if (backup_dir != NULL) {
		free(backup_dir);
	}
	if (last_search != NULL) {
		free(last_search);
	}
	if (last_replace != NULL) {
		free(last_replace);
	}
#ifdef ENABLE_SPELLER
	if (alt_speller != NULL) {
		free(alt_speller);
	}
#endif
	if (answer != NULL) {
		free(answer);
	}
	if (cutbuffer != NULL) {
		free_filestruct(cutbuffer);
	}
#ifdef DEBUG
	/* Free the memory associated with each open file buffer. */
	if (openfile != NULL) {
		free_openfilestruct(openfile);
	}
#endif
	if (syntaxstr != NULL) {
		free(syntaxstr);
	}
	/* Free the search and replace history lists. */
	if (searchage != NULL) {
		free_filestruct(searchage);
	}
	if (replaceage != NULL) {
		free_filestruct(replaceage);
	}
	if (homedir != NULL) {
		free(homedir);
	}
}

#endif /* DEBUG */
