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

#include <algorithm>
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

std::string last_search = "";
/* The last string we searched for. */
std::string last_replace = "";
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
/* The partition where we store a portion of the current file. */
std::list<OpenFile> openfiles;
/* The list of all open file buffers. */
std::list<OpenFile>::iterator openfile; // the current open file

char *matchbrackets = NULL;
/* The opening and closing brackets that can be found by bracket
 * searches. */

std::string whitespace;
/* The characters used when displaying the first characters of
 * tabs and spaces. */
int whitespace_len[2];
/* The length of these characters. */

bool nodelay_mode = false;
/* Are we in nodelay mode (checking for a cancel wile doing something */

std::string answer;
/* The answer string used by the statusbar prompt. */

ssize_t tabsize = -1;
/* The width of a tab in spaces.  The default value is set in
 * main(). */

std::string backup_dir = "";
/* The directory where we store backup files. */
const std::string locking_prefix = ".";
/* Prefix of how to store the vim-style lock file */
const std::string locking_suffix = ".swp";
/* Suffix of the vim-style lock file */

char *alt_speller = NULL;
/* The command to use for the alternate spell checker. */

SyntaxMap syntaxes;
/* The global list of color syntaxes. */
std::string syntaxstr;
/* The color syntax name specified on the command line. */

bool edit_refresh_needed = false;
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

int highlight_attribute = A_REVERSE;
/* The curses attribute we use for highlighting text (reverse video). */
std::string specified_color_combo[] = { };
/* The color combinations as specified in the rcfile. */
ColorPair interface_colors[] = { };
/* The processed color pairs for the interface elements. */

std::string homedir = "";
/* The user's home directory, from $HOME or /etc/passwd. */

/* Return the number of entries in the shortcut list s for a given menu. */
size_t length_of_list(int menu)
{
	size_t i = 0;

	for (auto f : allfuncs) {
		if ((f->menus & menu) != 0 && f->help.length() > 0) {
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
void toggle_replace_void(void)
{
}
void toggle_execute_void(void)
{
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
	for (auto s : sclist) {
		if ((s->menu & menu) && s->scfunc == func) {
			return s;
		}
	}

	DEBUG_LOG("Whoops, returning null given func " << func << " in menu " << menu);
	/* Otherwise... */
	return NULL;
}


/* Add a key combo to the shortcut list. */
void add_to_sclist(int menu, const std::string& scstring, void (*func)(void), int toggle)
{
	auto shortcut = new sc;
	shortcut->keystr = scstring;
	shortcut->menu = menu;
	shortcut->scfunc = func;
	shortcut->toggle = toggle;

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

/* Return a pointer to the function that is bound to the given key. */
FunctionPtr func_from_key(const Key& kbinput)
{
	const sc *s = get_shortcut(kbinput);

	return s ? s->scfunc : NULL;
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
	const char *insert_file_msg = N_("Insert File");
	const char *spell_msg = N_("To Spell");
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
	const char *pinot_spell_msg = N_("Invoke the spell checker, if available");
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
	const char *pinot_cut_till_eof_msg = N_("Cut from the cursor position to the end of the file");
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
	const char *pinot_formatter_msg = N_("Invoke formatter, if available");

// FIXME
#define IFSCHELP(help) help

	while (allfuncs.size() > 0) {
		auto f = allfuncs.front();
		allfuncs.pop_front();
		delete f;
	}

	/* TRANSLATORS: Try to keep the "Get Help" string to at most 10 characters. */
	add_to_funcs(do_help_void, MMOST, N_("Get Help"), IFSCHELP(pinot_help_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_cancel,
	              (MWHEREIS|MREPLACE|MREPLACEWITH|MGOTOLINE|MWRITEFILE|MINSERTFILE|MEXTCMD|MSPELL|MWHEREISFILE|MGOTODIR|MYESNO|MLINTER),
	              N_("Cancel"), IFSCHELP(pinot_cancel_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_exit, MMAIN, openfiles.size() > 0 ? N_("Close") : exit_msg, IFSCHELP(pinot_exit_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_exit, MBROWSER, exit_msg, IFSCHELP(pinot_exitbrowser_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_writeout_void, MMAIN, N_("WriteOut"), IFSCHELP(pinot_writeout_msg), GROUP_TOGETHER, NOVIEW);

	add_to_funcs(do_insertfile_void, MMAIN, N_("Read File"), IFSCHELP(pinot_insert_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_search, MMAIN|MBROWSER, whereis_msg, IFSCHELP(pinot_whereis_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_page_up, MMAIN|MHELP|MBROWSER, N_("Prev Page"), IFSCHELP(pinot_prevpage_msg), GROUP_TOGETHER, VIEW);
	add_to_funcs(do_page_down, MMAIN|MHELP|MBROWSER, N_("Next Page"), IFSCHELP(pinot_nextpage_msg), BLANK_AFTER, VIEW);

	add_to_funcs(do_page_up, MLINTER, prev_lint_msg, IFSCHELP(pinot_prevlint_msg), GROUP_TOGETHER, VIEW);
	add_to_funcs(do_page_down, MLINTER, next_lint_msg, IFSCHELP(pinot_nextlint_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_cut_text_void, MMAIN, N_("Cut Text"), IFSCHELP(pinot_cut_msg), GROUP_TOGETHER, NOVIEW);

	add_to_funcs(do_uncut_text, MMAIN, N_("UnCut Text"), IFSCHELP(pinot_uncut_msg), GROUP_TOGETHER, NOVIEW);

	add_to_funcs(do_cursorpos_void, MMAIN, N_("Cur Pos"), IFSCHELP(pinot_cursorpos_msg), GROUP_TOGETHER, VIEW);

	/* TRANSLATORS: Try to keep this at most 10 characters. */
	add_to_funcs(do_spell, MMAIN, spell_msg, IFSCHELP(pinot_spell_msg), BLANK_AFTER, NOVIEW);
	add_to_funcs(do_linter, MMAIN, lint_msg, IFSCHELP(pinot_lint_msg), BLANK_AFTER, NOVIEW);
	add_to_funcs(do_formatter, MMAIN, N_("Formatter"), IFSCHELP(pinot_formatter_msg), GROUP_TOGETHER, NOVIEW);

	add_to_funcs(do_first_line, (MMAIN|MHELP|MWHEREIS|MREPLACE|MREPLACEWITH|MGOTOLINE), N_("First Line"), IFSCHELP(pinot_firstline_msg), GROUP_TOGETHER, VIEW);
	add_to_funcs(do_last_line, (MMAIN|MHELP|MWHEREIS|MREPLACE|MREPLACEWITH|MGOTOLINE), N_("Last Line"), IFSCHELP(pinot_lastline_msg), BLANK_AFTER, VIEW);

	add_to_funcs(do_gotolinecolumn_void, (MMAIN|MWHEREIS), N_("Go To Line"), IFSCHELP(pinot_gotoline_msg), GROUP_TOGETHER, VIEW);

	/* TRANSLATORS: Try to keep this at most 10 characters. */
	add_to_funcs(do_cursorpos_void, MMAIN, N_("Cur Pos"), IFSCHELP(pinot_cursorpos_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_replace, MMAIN, _("Replace"), IFSCHELP(pinot_replace_msg), GROUP_TOGETHER, NOVIEW);

	add_to_funcs(do_mark, MMAIN, N_("Mark Text"), IFSCHELP(pinot_mark_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_research, (MMAIN|MBROWSER), whereis_next_msg, IFSCHELP(pinot_whereis_next_msg), BLANK_AFTER, VIEW);

	add_to_funcs(do_copy_text, MMAIN, N_("Copy Text"), IFSCHELP(pinot_copy_msg), GROUP_TOGETHER, NOVIEW);

	add_to_funcs(do_indent_void, MMAIN, N_("Indent Text"), IFSCHELP(pinot_indent_msg), GROUP_TOGETHER, NOVIEW);

	add_to_funcs(do_unindent, MMAIN, N_("Unindent Text"), IFSCHELP(pinot_unindent_msg), GROUP_TOGETHER, NOVIEW);

	add_to_funcs(do_undo, MMAIN, N_("Undo"), IFSCHELP(pinot_undo_msg), GROUP_TOGETHER, NOVIEW);
	add_to_funcs(do_redo, MMAIN, N_("Redo"), IFSCHELP(pinot_redo_msg), BLANK_AFTER, NOVIEW);

	add_to_funcs(do_execute_command, MMAIN, N_("Exec Cmd"), IFSCHELP(pinot_execute_msg), GROUP_TOGETHER, NOVIEW);

	add_to_funcs(do_right, MMAIN, N_("Forward"), IFSCHELP(pinot_forward_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_right, MBROWSER, N_("Forward"), IFSCHELP(pinot_forwardfile_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_left, MMAIN, N_("Back"), IFSCHELP(pinot_back_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_left, MBROWSER, N_("Back"), IFSCHELP(pinot_backfile_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_prev_word_void, MMAIN, N_("Prev Word"), IFSCHELP(pinot_prevword_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_next_word_void, MMAIN, N_("Next Word"), IFSCHELP(pinot_nextword_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_up_void, (MMAIN|MHELP|MBROWSER), N_("Prev Line"), IFSCHELP(pinot_prevline_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_down_void, (MMAIN|MHELP|MBROWSER), N_("Next Line"), IFSCHELP(pinot_nextline_msg), BLANK_AFTER, VIEW);

	add_to_funcs(do_home, MMAIN, N_("Home"), IFSCHELP(pinot_home_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_end, MMAIN, N_("End"), IFSCHELP(pinot_end_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_find_bracket, MMAIN, _("Find Other Bracket"), IFSCHELP(pinot_bracket_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_scroll_up, MMAIN, N_("Scroll Up"), IFSCHELP(pinot_scrollup_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_scroll_down, MMAIN, N_("Scroll Down"), IFSCHELP(pinot_scrolldown_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(switch_to_prev_buffer_void, MMAIN, _("Previous File"), IFSCHELP(pinot_prevfile_msg), GROUP_TOGETHER, VIEW);
	add_to_funcs(switch_to_next_buffer_void, MMAIN, N_("Next File"), IFSCHELP(pinot_nextfile_msg), BLANK_AFTER, VIEW);

	add_to_funcs(do_verbatim_input, MMAIN, N_("Verbatim Input"), IFSCHELP(pinot_verbatim_msg), GROUP_TOGETHER, NOVIEW);

	add_to_funcs(do_tab, MMAIN, N_("Tab"), IFSCHELP(pinot_tab_msg), GROUP_TOGETHER, NOVIEW);
	add_to_funcs(do_enter_void, MMAIN, N_("Enter"), IFSCHELP(pinot_enter_msg), GROUP_TOGETHER, NOVIEW);
	add_to_funcs(do_delete, MMAIN, N_("Delete"), IFSCHELP(pinot_delete_msg), GROUP_TOGETHER, NOVIEW);
	add_to_funcs(do_backspace, MMAIN, N_("Backspace"), IFSCHELP(pinot_backspace_msg), GROUP_TOGETHER, NOVIEW);

	add_to_funcs(do_cut_till_eof, MMAIN, N_("CutTillEnd"), IFSCHELP(pinot_cut_till_eof_msg), BLANK_AFTER, NOVIEW);

	add_to_funcs(do_wordlinechar_count, MMAIN, N_("Word Count"), IFSCHELP(pinot_wordcount_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(total_refresh, (MMAIN|MHELP), refresh_msg, IFSCHELP(pinot_refresh_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_suspend_void, MMAIN, N_("Suspend"), IFSCHELP(pinot_suspend_msg), BLANK_AFTER, VIEW);

	add_to_funcs(case_sens_void, (MWHEREIS|MREPLACE), N_("Case Sens"), IFSCHELP(pinot_case_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(backwards_void, (MWHEREIS|MREPLACE), N_("Backwards"), IFSCHELP(pinot_reverse_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(regexp_void, (MWHEREIS|MREPLACE), N_("Regexp"), IFSCHELP(pinot_regexp_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(get_history_older_void, (MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE), N_("PrevHist"), IFSCHELP(pinot_prev_history_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(get_history_newer_void, (MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE), N_("NextHist"), IFSCHELP(pinot_next_history_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(toggle_replace_void, MWHEREIS, N_("Replace"), IFSCHELP(pinot_whereis_msg), GROUP_TOGETHER, VIEW);
	add_to_funcs(toggle_replace_void, MREPLACE, N_("No Replace"), IFSCHELP(pinot_whereis_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(gototext_void, MGOTOLINE, N_("Go To Text"), IFSCHELP(pinot_whereis_msg), BLANK_AFTER, VIEW);

	add_to_funcs(to_files_void, MINSERTFILE, N_("To Files"), IFSCHELP(pinot_tofiles_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(dos_format_void, MWRITEFILE, N_("DOS Format"), IFSCHELP(pinot_dos_msg), GROUP_TOGETHER, NOVIEW);
	add_to_funcs(mac_format_void, MWRITEFILE, N_("Mac Format"), IFSCHELP(pinot_mac_msg), GROUP_TOGETHER, NOVIEW);
	add_to_funcs(append_void, MWRITEFILE, N_("Append"), IFSCHELP(pinot_append_msg), GROUP_TOGETHER, NOVIEW);
	add_to_funcs(prepend_void, MWRITEFILE, N_("Prepend"), IFSCHELP(pinot_prepend_msg), GROUP_TOGETHER, NOVIEW);
	add_to_funcs( backup_file_void, MWRITEFILE, N_("Backup File"), IFSCHELP(pinot_backup_msg), GROUP_TOGETHER, NOVIEW);

	add_to_funcs(toggle_execute_void, MINSERTFILE, N_("Execute Command"), IFSCHELP(pinot_execute_msg), GROUP_TOGETHER, NOVIEW);
	add_to_funcs(toggle_execute_void, MEXTCMD, N_("Read File"), IFSCHELP(pinot_insert_msg), GROUP_TOGETHER, NOVIEW);

	add_to_funcs(new_buffer_void, MINSERTFILE|MEXTCMD, N_("New Buffer"), IFSCHELP(pinot_multibuffer_msg), GROUP_TOGETHER, NOVIEW);

	add_to_funcs(do_insertfile_void, MEXTCMD, insert_file_msg, IFSCHELP(pinot_insert_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(edit_refresh, MHELP, refresh_msg, pinot_refresh_msg, GROUP_TOGETHER, VIEW);

	add_to_funcs(do_exit, MHELP, exit_msg, IFSCHELP(pinot_exit_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_first_file, (MBROWSER|MWHEREISFILE), N_("First File"), IFSCHELP(pinot_firstfile_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(do_last_file, (MBROWSER|MWHEREISFILE), N_("Last File"), IFSCHELP(pinot_lastfile_msg), GROUP_TOGETHER, VIEW);

	add_to_funcs(goto_dir_void, MBROWSER, N_("Go To Dir"), IFSCHELP(pinot_gotodir_msg), GROUP_TOGETHER, VIEW);

	empty_sclist();

	add_to_sclist(MMOST, "^G", do_help_void, 0);
	add_to_sclist(MMOST, "F1", do_help_void, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "^X", do_exit, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "F2", do_exit, 0);
	add_to_sclist(MMAIN, "^_", do_gotolinecolumn_void, 0);
	add_to_sclist(MMAIN, "F13", do_gotolinecolumn_void, 0);
	add_to_sclist(MMAIN, "M-G", do_gotolinecolumn_void, 0);
	add_to_sclist(MMAIN, "^O", do_writeout_void, 0);
	add_to_sclist(MMAIN, "F3", do_writeout_void, 0);
	add_to_sclist(MMAIN, "^R", do_insertfile_void, 0);
	add_to_sclist(MMAIN, "F5", do_insertfile_void, 0);
	add_to_sclist(MMAIN, "Insert", do_insertfile_void, 0);
	add_to_sclist(MMAIN|MBROWSER, "^W", do_search, 0);
	add_to_sclist(MMAIN|MBROWSER, "F6", do_search, 0);
	add_to_sclist(MMAIN|MBROWSER|MHELP|MWHEREISFILE|MLINTER, "^Y", do_page_up, 0);
	add_to_sclist(MMAIN|MBROWSER|MHELP|MWHEREISFILE|MLINTER, "F7", do_page_up, 0);
	add_to_sclist(MMAIN|MBROWSER|MHELP|MWHEREISFILE|MLINTER, "PageUp", do_page_up, 0);
	add_to_sclist(MMAIN|MBROWSER|MHELP|MWHEREISFILE|MLINTER, "^V", do_page_down, 0);
	add_to_sclist(MMAIN|MBROWSER|MHELP|MWHEREISFILE|MLINTER, "F8", do_page_down, 0);
	add_to_sclist(MMAIN|MBROWSER|MHELP|MWHEREISFILE|MLINTER, "PageDown", do_page_down, 0);
	add_to_sclist(MMOST, "^K", do_cut_text_void, 0);
	add_to_sclist(MMOST, "F9", do_cut_text_void, 0);
	add_to_sclist(MMAIN, "^U", do_uncut_text, 0);
	add_to_sclist(MMAIN, "F10", do_uncut_text, 0);
	add_to_sclist(MMAIN, "^C", do_cursorpos_void, 0);
	add_to_sclist(MMAIN, "F11", do_cursorpos_void, 0);
	add_to_sclist(MMAIN, "^T", do_spell, 0);
	add_to_sclist(MMAIN, "F12", do_spell, 0);
	add_to_sclist(MMAIN, "^T", do_linter, 0);
	add_to_sclist(MMAIN, "F12", do_linter, 0);
	add_to_sclist(MMAIN, "^T", do_formatter, 0);
	add_to_sclist(MMAIN, "F12", do_formatter, 0);
	add_to_sclist(MMAIN, "^\\", do_replace, 0);
	add_to_sclist(MMAIN, "F14", do_replace, 0);
	add_to_sclist(MMAIN, "M-R", do_replace, 0);
	add_to_sclist(MWHEREIS|MREPLACE, "^R", toggle_replace_void, 0);
	add_to_sclist(MWHEREIS, "^T", do_gotolinecolumn_void, 0);
	add_to_sclist(MMAIN, "^^", do_mark, 0);
	add_to_sclist(MMAIN, "F15", do_mark, 0);
	add_to_sclist(MMAIN, "M-A", do_mark, 0);
	add_to_sclist(MMAIN|MBROWSER, "M-W", do_research, 0);
	add_to_sclist(MMAIN|MBROWSER, "F16", do_research, 0);
	add_to_sclist(MMAIN, "M-^", do_copy_text, 0);
	add_to_sclist(MMAIN, "M-6", do_copy_text, 0);
	add_to_sclist(MMAIN, "M-}", do_indent_void, 0);
	add_to_sclist(MMAIN, "M-{", do_unindent, 0);
	add_to_sclist(MMAIN, "M-U", do_undo, 0);
	add_to_sclist(MMAIN, "M-E", do_redo, 0);
	add_to_sclist(MMOST, "^F", do_right, 0);
	add_to_sclist(MMOST, "Right", do_right, 0);
	add_to_sclist(MMOST, "^B", do_left, 0);
	add_to_sclist(MMOST, "Left", do_left, 0);

	add_to_sclist(MMOST, "^@", do_next_word_void, 0);
	add_to_sclist(MMOST, "M- ", do_prev_word_void, 0);

	add_to_sclist(MMAIN, "^Q", xon_complaint, 0);
	add_to_sclist(MMAIN, "^S", xoff_complaint, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "^P", do_up_void, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "Up", do_up_void, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "^N", do_down_void, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "Down", do_down_void, 0);
	add_to_sclist(MMOST, "^A", do_home, 0);
	add_to_sclist(MMOST, "Home", do_home, 0);
	add_to_sclist(MMOST, "^E", do_end, 0);
	add_to_sclist(MMOST, "End", do_end, 0);
	add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE, "^P", get_history_older_void, 0);
	add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE, "Up", get_history_older_void, 0);
	add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE, "^N", get_history_newer_void, 0);
	add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE, "Down", get_history_newer_void, 0);
	add_to_sclist(MWHEREIS|MREPLACE, "M-C", case_sens_void, 0);
	add_to_sclist(MWHEREIS|MREPLACE, "M-B", backwards_void, 0);
	add_to_sclist(MWHEREIS|MREPLACE, "M-R", regexp_void, 0);

	add_to_sclist(MMAIN|MHELP, "M-\\", do_first_line, 0);
	add_to_sclist(MMAIN|MHELP, "M-|", do_first_line, 0);
	add_to_sclist(MMAIN|MHELP, "M-/", do_last_line, 0);
	add_to_sclist(MMAIN|MHELP, "M-?", do_last_line, 0);
	add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MGOTOLINE|MHELP, "^Y", do_first_line, 0);
	add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MGOTOLINE|MHELP, "^V", do_last_line, 0);

	add_to_sclist(MBROWSER|MWHEREISFILE, "M-\\", do_first_file, 0);
	add_to_sclist(MBROWSER|MWHEREISFILE, "M-|", do_first_file, 0);
	add_to_sclist(MBROWSER|MWHEREISFILE, "M-/", do_last_file, 0);
	add_to_sclist(MBROWSER|MWHEREISFILE, "M-?", do_last_file, 0);
	add_to_sclist(MBROWSER|MWHEREISFILE, "^_", goto_dir_void, 0);
	add_to_sclist(MBROWSER|MWHEREISFILE, "F13", goto_dir_void, 0);
	add_to_sclist(MBROWSER|MWHEREISFILE, "M-G", goto_dir_void, 0);
	add_to_sclist(MMAIN, "M-]", do_find_bracket, 0);
	add_to_sclist(MMAIN, "M--", do_scroll_up, 0);
	add_to_sclist(MMAIN, "M-_", do_scroll_up, 0);
	add_to_sclist(MMAIN, "M-+", do_scroll_down, 0);
	add_to_sclist(MMAIN, "M-=", do_scroll_down, 0);

	add_to_sclist(MMAIN, "M-<", switch_to_prev_buffer_void, 0);
	add_to_sclist(MMAIN, "M-,", switch_to_prev_buffer_void, 0);
	add_to_sclist(MMAIN, "M->", switch_to_next_buffer_void, 0);
	add_to_sclist(MMAIN, "M-.", switch_to_next_buffer_void, 0);
	add_to_sclist(MMOST, "M-V", do_verbatim_input, 0);
	add_to_sclist(MMAIN, "M-T", do_cut_till_eof, 0);
	add_to_sclist(MMAIN, "M-D", do_wordlinechar_count, 0);
	add_to_sclist(MMAIN, "M-X", do_toggle_void, NO_HELP);
	add_to_sclist(MMAIN, "M-C", do_toggle_void, CONST_UPDATE);
	add_to_sclist(MMAIN, "M-O", do_toggle_void, MORE_SPACE);
	add_to_sclist(MMAIN, "M-S", do_toggle_void, SMOOTH_SCROLL);
	add_to_sclist(MMAIN, "M-P", do_toggle_void, WHITESPACE_DISPLAY);
	add_to_sclist(MMAIN, "M-Y", do_toggle_void, NO_COLOR_SYNTAX);
	add_to_sclist(MMAIN, "M-H", do_toggle_void, SMART_HOME);
	add_to_sclist(MMAIN, "M-I", do_toggle_void, AUTOINDENT);
	add_to_sclist(MMAIN, "M-K", do_toggle_void, CUT_TO_END);
	add_to_sclist(MMAIN, "M-L", do_toggle_void, NO_WRAP);
	add_to_sclist(MMAIN, "M-Q", do_toggle_void, TABS_TO_SPACES);
	add_to_sclist(MMAIN, "M-B", do_toggle_void, BACKUP_FILE);
	add_to_sclist(MMAIN, "M-F", do_toggle_void, MULTIBUFFER);
	add_to_sclist(MMAIN, "M-N", do_toggle_void, NO_CONVERT);
	add_to_sclist(MMAIN, "M-Z", do_toggle_void, SUSPEND);
	add_to_sclist(MMAIN, "M-$", do_toggle_void, SOFTWRAP);
	add_to_sclist(MHELP|MBROWSER, "^C", do_exit, 0);
	add_to_sclist(MHELP, "^G", do_exit, 0);
	add_to_sclist(MGOTOLINE, "^T",  gototext_void, 0);
	add_to_sclist(MINSERTFILE|MEXTCMD, "M-F", new_buffer_void, 0);
	add_to_sclist((MWHEREIS|MREPLACE|MREPLACEWITH|MGOTOLINE|MWRITEFILE|MINSERTFILE|MEXTCMD|MSPELL|MWHEREISFILE|MGOTODIR|MYESNO|MLINTER), "^C", do_cancel, 0);
	add_to_sclist(MWRITEFILE, "M-D", dos_format_void, 0);
	add_to_sclist(MWRITEFILE, "M-M", mac_format_void, 0);
	add_to_sclist(MWRITEFILE, "M-A", append_void, 0);
	add_to_sclist(MWRITEFILE, "M-P", prepend_void, 0);
	add_to_sclist(MWRITEFILE, "M-B", backup_file_void, 0);
	add_to_sclist(MWRITEFILE|MINSERTFILE, "^T", to_files_void, 0);
	add_to_sclist(MINSERTFILE|MEXTCMD, "^X", toggle_execute_void, 0);
	add_to_sclist(MMAIN, "^Z", do_suspend_void, 0);
	add_to_sclist(MMAIN|MHELP, "^L", total_refresh, 0);
	add_to_sclist(MMOST, "Tab", do_tab, 0);
	add_to_sclist(MMOST, "Enter", do_enter_void, 0);
	add_to_sclist(MMOST, "KPEnter", do_enter_void, 0);
	add_to_sclist(MMOST, "^D", do_delete, 0);
	add_to_sclist(MMOST, "Delete", do_delete, 0);
	add_to_sclist(MMOST, "Backspace", do_backspace, 0);

#ifdef DEBUG
	print_sclist();
#endif

}

void set_lint_or_format_shortcuts(void)
{
	if (openfile->syntax->formatter != "") {
		replace_scs_for(do_spell, do_formatter);
		replace_scs_for(do_linter, do_formatter);
	} else {
		replace_scs_for(do_spell, do_linter);
		replace_scs_for(do_formatter, do_linter);
	}
}

void set_spell_shortcuts(void)
{
	replace_scs_for(do_formatter, do_spell);
	replace_scs_for(do_linter, do_spell);
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
    shortcut struct with the corresponding function filled in. */
sc *strtosc(std::string input)
{
	sc *s = new sc;

	// Convert input to lowercase for case-insensitive comparisons
	std::transform(input.begin(), input.end(), input.begin(), ::tolower);

	if (input == "help") {
		s->scfunc = do_help_void;
	} else if (input == "cancel") {
		s->scfunc = do_cancel;
	} else if (input == "exit") {
		s->scfunc = do_exit;
	} else if (input == "writeout") {
		s->scfunc = do_writeout_void;
	} else if (input == "insert") {
		s->scfunc = do_insertfile_void;
	} else if (input == "execute") {
		s->scfunc = do_execute_command;
	} else if (input == "whereis") {
		s->scfunc = do_search;
	} else if (input == "up") {
		s->scfunc = do_up_void;
	} else if (input == "down") {
		s->scfunc = do_down_void;
	} else if (input == "pageup" || input == "prevpage") {
		s->scfunc = do_page_up;
	} else if (input == "pagedown" || input == "nextpage") {
		s->scfunc = do_page_down;
	} else if (input == "cut") {
		s->scfunc = do_cut_text_void;
	} else if (input == "uncut") {
		s->scfunc = do_uncut_text;
	} else if (input == "cutrestoffile") {
		s->scfunc = do_cut_till_eof;
	} else if (input == "curpos" || input == "cursorpos") {
		s->scfunc = do_cursorpos_void;
	} else if (input == "firstline") {
		s->scfunc = do_first_line;
	} else if (input == "lastline") {
		s->scfunc = do_last_line;
	} else if (input == "gotoline") {
		s->scfunc = do_gotolinecolumn_void;
	} else if (input == "replace") {
		s->scfunc = do_replace;
	} else if (input == "mark") {
		s->scfunc = do_mark;
	} else if (input == "searchagain" || input == "research") {
		s->scfunc = do_research;
	} else if (input == "copytext") {
		s->scfunc = do_copy_text;
	} else if (input == "indent") {
		s->scfunc = do_indent_void;
	} else if (input == "unindent") {
		s->scfunc = do_unindent;
	} else if (input == "scrollup") {
		s->scfunc = do_scroll_up;
	} else if (input == "scrolldown") {
		s->scfunc = do_scroll_down;
	} else if (input == "prevword") {
		s->scfunc = do_prev_word_void;
	} else if (input == "nextword") {
		s->scfunc = do_next_word_void;
	} else if (input == "findbracket") {
		s->scfunc = do_find_bracket;
	} else if (input == "wordcount") {
		s->scfunc = do_wordlinechar_count;
	} else if (input == "suspend") {
		s->scfunc = do_suspend_void;
	} else if (input == "undo") {
		s->scfunc = do_undo;
	} else if (input == "redo") {
		s->scfunc = do_redo;
	} else if (input == "prevhistory") {
		s->scfunc = get_history_older_void;
	} else if (input == "nexthistory") {
		s->scfunc = get_history_newer_void;
	} else if (input == "nohelp") {
		s->scfunc = do_toggle_void;
		s->toggle = NO_HELP;
	} else if (input == "constupdate") {
		s->scfunc = do_toggle_void;
		s->toggle = CONST_UPDATE;
	} else if (input == "morespace") {
		s->scfunc = do_toggle_void;
		s->toggle = MORE_SPACE;
	} else if (input == "smoothscroll") {
		s->scfunc = do_toggle_void;
		s->toggle = SMOOTH_SCROLL;
	} else if (input == "whitespacedisplay") {
		s->scfunc = do_toggle_void;
		s->toggle = WHITESPACE_DISPLAY;
	} else if (input == "nosyntax") {
		s->scfunc = do_toggle_void;
		s->toggle = NO_COLOR_SYNTAX;
	} else if (input == "smarthome") {
		s->scfunc = do_toggle_void;
		s->toggle = SMART_HOME;
	} else if (input == "autoindent") {
		s->scfunc = do_toggle_void;
		s->toggle = AUTOINDENT;
	} else if (input == "cuttoend") {
		s->scfunc = do_toggle_void;
		s->toggle = CUT_TO_END;
	} else if (input == "nowrap") {
		s->scfunc = do_toggle_void;
		s->toggle = NO_WRAP;
	} else if (input == "softwrap") {
		s->scfunc = do_toggle_void;
		s->toggle = SOFTWRAP;
	} else if (input == "tabstospaces") {
		s->scfunc = do_toggle_void;
		s->toggle = TABS_TO_SPACES;
	} else if (input == "backupfile") {
		s->scfunc = do_toggle_void;
		s->toggle = BACKUP_FILE;
	} else if (input == "multibuffer") {
		s->scfunc = do_toggle_void;
		s->toggle = MULTIBUFFER;
	} else if (input == "noconvert") {
		s->scfunc = do_toggle_void;
		s->toggle = NO_CONVERT;
	} else if (input == "suspendable") {
		s->scfunc = do_toggle_void;
		s->toggle = SUSPEND;
	} else if (input == "right" || input == "forward") {
		s->scfunc = do_right;
	} else if (input == "left" || input == "back") {
		s->scfunc = do_left;
	} else if (input == "up" || input == "prevline") {
		s->scfunc = do_up_void;
	} else if (input == "down" || input == "nextline") {
		s->scfunc = do_down_void;
	} else if (input == "home") {
		s->scfunc = do_home;
	} else if (input == "end") {
		s->scfunc = do_end;
	} else if (input == "prevbuf") {
		s->scfunc = switch_to_prev_buffer_void;
	} else if (input == "nextbuf") {
		s->scfunc = switch_to_next_buffer_void;
	} else if (input == "verbatim") {
		s->scfunc = do_verbatim_input;
	} else if (input == "tab") {
		s->scfunc = do_tab;
	} else if (input == "enter") {
		s->scfunc = do_enter_void;
	} else if (input == "delete") {
		s->scfunc = do_delete;
	} else if (input == "backspace") {
		s->scfunc = do_backspace;
	} else if (input == "refresh") {
		s->scfunc = total_refresh;
	} else if (input == "casesens") {
		s->scfunc = case_sens_void;
	} else if (input == "regexp" || input == "regex") {
		s->scfunc = regexp_void;
	} else if (input == "backwards") {
		s->scfunc = backwards_void;
	} else if (input == "togglereplace") {
		s->scfunc = toggle_replace_void;
	} else if (input == "gototext") {
		s->scfunc = gototext_void;
	} else if (input == "dosformat") {
		s->scfunc = dos_format_void;
	} else if (input == "macformat") {
		s->scfunc = mac_format_void;
	} else if (input == "append") {
		s->scfunc = append_void;
	} else if (input == "prepend") {
		s->scfunc = prepend_void;
	} else if (input == "backup") {
		s->scfunc = backup_file_void;
	} else if (input == "toggleexecute") {
		s->scfunc = toggle_execute_void;
	} else if (input == "newbuffer" || input == "togglebuffer") {
		s->scfunc = new_buffer_void;
	} else if (input == "browser" || input == "files") {
		s->scfunc = to_files_void;
	} else if (input == "gotodir") {
		s->scfunc = goto_dir_void;
	} else if (input == "firstfile") {
		s->scfunc = do_first_file;
	} else if (input == "lastfile") {
		s->scfunc = do_last_file;
	} else if (input == "tospell" || input == "speller") {
		s->scfunc = do_spell;
	} else {
		delete s;
		return NULL;
	}

	return s;

}

/* Same thing as abnove but for the menu */
int strtomenu(std::string input)
{
	// Convert input to lowercase for case-insensitive comparisons
	std::transform(input.begin(), input.end(), input.begin(), ::tolower);

	if (input == "all") {
		return MMOST;
	} else if (input == "main") {
		return MMAIN;
	} else if (input == "search") {
		return MWHEREIS;
	} else if (input == "replace") {
		return MREPLACE;
	} else if (input == "replacewith" || input == "replace2") {
		return MREPLACEWITH;
	} else if (input == "gotoline") {
		return MGOTOLINE;
	} else if (input == "writeout") {
		return MWRITEFILE;
	} else if (input == "insert") {
		return MINSERTFILE;
	} else if (input == "extcmd" || input == "externalcmd") {
		return MEXTCMD;
	} else if (input == "help") {
		return MHELP;
	} else if (input == "spell") {
		return MSPELL;
	} else if (input == "linter") {
		return MLINTER;
	} else if (input == "browser") {
		return MBROWSER;
	} else if (input == "whereisfile") {
		return MWHEREISFILE;
	} else if (input == "gotodir") {
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

	if (alt_speller != NULL) {
		free(alt_speller);
	}
	if (cutbuffer != NULL) {
		free_filestruct(cutbuffer);
	}
	/* Free the search and replace history lists. */
	if (searchage != NULL) {
		free_filestruct(searchage);
	}
	if (replaceage != NULL) {
		free_filestruct(replaceage);
	}
}

#endif /* DEBUG */
