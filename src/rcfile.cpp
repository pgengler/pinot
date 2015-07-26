/**************************************************************************
 *   rcfile.c                                                             *
 *                                                                        *
 *   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009   *
 *   Free Software Foundation, Inc.                                       *
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
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

#include <ctype.h>
#include <errno.h>
#include <glob.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

using pinot::string;

std::vector<rcoption> rcopts = {
	{"boldtext", BOLD_TEXT, false},
	{"const", CONST_UPDATE, false},
	{"fill", 0, true},
	{"locking", LOCKING, false},
	{"multibuffer", MULTIBUFFER, false},
	{"morespace", MORE_SPACE, false},
	{"nofollow", NOFOLLOW_SYMLINKS, false},
	{"nohelp", NO_HELP, false},
	{"nonewlines", NO_NEWLINES, false},
	{"nowrap", NO_WRAP, false},
	{"preserve", PRESERVE, false},
	{"rebindkeypad", REBIND_KEYPAD, false},
	{"regexp", USE_REGEXP, false},
	{"speller", 0, false},
	{"suspend", SUSPEND, false},
	{"tabsize", 0, true},
	{"tempfile", TEMP_FILE, false},
	{"view", VIEW_MODE, false},
	{"autoindent", AUTOINDENT, true},
	{"backup", BACKUP_FILE, false},
	{"allow_insecure_backup", INSECURE_BACKUP, false},
	{"backupdir", 0, false},
	{"backwards", BACKWARDS_SEARCH, false},
	{"casesensitive", CASE_SENSITIVE, false},
	{"cut", CUT_TO_END, false},
	{"historylog", HISTORYLOG, false},
	{"matchbrackets", 0, true},
	{"noconvert", NO_CONVERT, false},
	{"poslog", POS_HISTORY, false},
	{"quiet", QUIET, false},
	{"quickblank", QUICK_BLANK, false},
	{"smarthome", SMART_HOME, false},
	{"smooth", SMOOTH_SCROLL, false},
	{"tabstospaces", TABS_TO_SPACES, true},
	{"whitespace", 0, false},
	{"wordbounds", WORD_BOUNDS, false},
	{"softwrap", SOFTWRAP, true},
	{"titlecolor", 0, false},
	{"statuscolor", 0, false},
	{"keycolor", 0, false},
	{"functioncolor", 0, false},
};

static bool errors = false;
/* Whether we got any errors while parsing an rcfile. */
static size_t lineno = 0;
/* If we did, the line number where the last error occurred. */
static string pinotrc;
/* The path to the rcfile we're parsing. */
static Syntax *current_syntax = nullptr;
/* current syntax being processed */

/* We have an error in some part of the rcfile.  Print the error message
 * on stderr, and then make the user hit Enter to continue starting pinot. */
void rcfile_error(const char *msg, ...)
{
	va_list ap;

	if (ISSET(QUIET)) {
		return;
	}

	fprintf(stderr, "\n");
	if (lineno > 0) {
		errors = true;
		fprintf(stderr, _("Error in %s on line %lu: "), pinotrc.c_str(), (unsigned long)lineno);
	}

	va_start(ap, msg);
	vfprintf(stderr, _(msg), ap);
	va_end(ap);

	fprintf(stderr, "\n");
}

/* Compile the regular expression regex to see if it's valid.  Return
 * true if it is, or false otherwise. */
bool nregcomp(string regex, int cflags)
{
	regex_t preg;
	int rc = regcomp(&preg, regex.c_str(), REG_EXTENDED | cflags);

	if (rc != 0) {
		size_t len = regerror(rc, &preg, NULL, 0);
		char *str = charalloc(len);

		regerror(rc, &preg, str, len);
		rcfile_error(N_("Bad regex \"%s\": %s"), regex.c_str(), str);
		free(str);
	}

	regfree(&preg);
	return (rc == 0);
}

string rest(std::stringstream& stream)
{
	auto max_size = stream.str().length();
	char *buffer = new char[max_size + 1];
	stream.get(buffer, max_size);
	string str(buffer);
	delete[] buffer;

	return str;
}

void skip_whitespace(std::stringstream& stream)
{
	// Eliminate leading whitespace
	while (!stream.eof() && isspace(stream.peek())) {
		stream.get();
	}
}

string parse_regex(std::stringstream& line)
{
	string regex;

	if (line.peek() != '/') {
		rcfile_error(N_("Regular expressions must start with a /"));
		return "";
	}
	// Throw away opening slash
	line.get();

	char last_char = '\0';
	bool read_double_backslash = false;
	while (line.good()) {
		if (!read_double_backslash && last_char == '\\' && line.peek() == '/') {
			// Allow internal slashes to be escaped, but don't save the escaping backslash
			regex = regex.substr(0, regex.length() - 1);
		} else if (line.peek() == '/') {
			// Throw away trailing slash and return since we've reached the end
			line.get();
			return regex;
		}
		last_char = line.get();
		// If we find a double-backslash treat it as a whole escape; don't allow the
		// second backslash to escape a forward slash
		if (last_char == '\\' && regex.back() == '\\' && !read_double_backslash) {
			read_double_backslash = true;
		} else {
			read_double_backslash = false;
		}
		regex += last_char;
	}

	// If we got here we reached the end of the line without finding a terminating slash
	rcfile_error(N_("Regular expressions must end with a /"));

	return "";
}

/* Parse the next syntax string from the line and add it to the global list of color syntaxes. */
void parse_syntax(std::stringstream& line)
{
	string name;
	line >> name;
	name = name.trim();

	if (name.empty()) {
		rcfile_error(N_("Missing syntax name"));
		return;
	}

	// Check for existing syntax with this name
	auto existing_syntax = syntaxes[name];
	if (existing_syntax) {
		DEBUG_LOG("Found existing syntax with name \"" << name << "\"; overriding with new definition");
		syntaxes.erase(name);
		delete existing_syntax;
		existing_syntax = nullptr;
	}

	current_syntax = new Syntax(name);

	DEBUG_LOG("Starting a new syntax type: \"" << name << '"');

	/* The "none" syntax is the same as not having a syntax at all, so
	 * we can't assign any extensions or colors to it. */
	if (current_syntax->desc == "none") {
		rcfile_error(N_("The \"none\" syntax is reserved"));
		return;
	}

	syntaxes[name] = current_syntax;
}

/* Parse an optional "extends" line in a syntax. */
void parse_extends(std::stringstream& line)
{
	if (!current_syntax) {
		rcfile_error(N_("Cannot use 'extends' command without a 'syntax' command"));
		return;
	}

	string syntax_name;
	line >> syntax_name;
	syntax_name = syntax_name.trim();

	if (syntax_name.empty()) {
		rcfile_error(N_("Missing name of syntax to extend"));
		return;
	}

	current_syntax->extends.push_back(syntax_name);
}

#ifdef HAVE_LIBMAGIC
void parse_magic(std::stringstream& line)
{
	if (!current_syntax) {
		rcfile_error(N_("Cannot add a magic string regex without a syntax command"));
		return;
	}

	string regex = parse_regex(line);

	if (regex.empty()) {
		rcfile_error(N_("Missing magic string name"));
		return;
	}

	DEBUG_LOG("Starting a magic type: \"" << regex << '"');

	/* Save the regex if it's valid. */
	if (nregcomp(regex, REG_NOSUB)) {
		auto newext = new SyntaxMatch(regex);
		current_syntax->magics.push_back(newext);
	}
}
#endif /* HAVE_LIBMAGIC */

bool is_universal_function(void (*func)(void))
{
	if (func == do_left || func == do_right || func == do_home || func == do_end ||
	    func == do_prev_word_void || func == do_next_word_void || func == do_verbatim_input ||
	    func == do_cut_text_void || func == do_delete || func == do_backspace || func == do_tab || func == do_enter_void) {
		return true;
	}
	return false;
}

void parse_keybinding(std::stringstream& line)
{
	string key_name;
	line >> key_name;

	if (key_name.empty()) {
		rcfile_error(N_("Missing key name"));
		return;
	}

	if (key_name[0] != 'M' && key_name[0] != '^' && key_name[0] != 'F') {
		rcfile_error(N_("keybindings must begin with \"^\", \"M\", or \"F\""));
		return;
	}

	string func_name;
	line >> func_name;

	if (func_name.empty()) {
		rcfile_error(N_("Must specify function to bind key to"));
		return;
	}

	string menu_name;
	line >> menu_name;

	if (menu_name.empty()) {
		/* Note to translators, do not translate the word "all"
		   in the sentence below, everything else is fine */
		rcfile_error(N_("Must specify menu to bind key to (or \"all\")"));
		return;
	}

	auto menu = strtomenu(menu_name);
	auto newsc = strtosc(func_name);
	if (!newsc) {
		rcfile_error(N_("Could not map name \"%s\" to a function"), func_name.c_str());
		return;
	}

	if (menu < 1) {
		rcfile_error(N_("Could not map name \"%s\" to a menu"), menu_name.c_str());
		return;
	}

	DEBUG_LOG("newsc now address " << static_cast<void *>(&newsc) << ", menu func assigned = " << newsc->scfunc << ", menu = " << menu);

	int mask = 0;
	for (auto f : allfuncs) {
		if (f->scfunc == newsc->scfunc) {
			mask |= f->menus;
		}
	}

	if (is_universal_function(newsc->scfunc)) {
		menu = menu & MMOST;
	} else {
		menu = menu & mask;
	}

	if (!menu) {
		rcfile_error(N_("Function '%s' does not exist in menu '%s'"), func_name.c_str(), menu_name.c_str());
		free(newsc);
		return;
	}

	newsc->keystr = key_name;
	newsc->menu = menu;
	DEBUG_LOG("s->keystr = \"" << newsc->keystr << '"');

	/* Remove previous bindings for this key/menu combination */
	for (auto s : sclist) {
		if (((s->menu & newsc->menu)) && s->keystr == newsc->keystr) {
			s->menu &= ~newsc->menu;
			DEBUG_LOG("replaced menu entry " << s->menu);
		}
	}
	sclist.push_back(newsc);
}

/* Let user unbind a sequence from a given (or all) menus */
void parse_unbinding(std::stringstream& line)
{
	string key_name;
	line >> key_name;
	if (key_name.empty()) {
		rcfile_error(N_("Missing key name"));
		return;
	}

	DEBUG_LOG("Starting unbinding code");

	if (key_name[0] != 'M' && key_name[0] != '^' && key_name[0] != 'F') {
		rcfile_error(N_("keybindings must begin with \"^\", \"M\", or \"F\""));
		return;
	}

	string menu_name;
	line >> menu_name;

	if (menu_name.empty()) {
		/* Note to translators, do not translate the word "all"
		   in the sentence below, everything else is fine */
		rcfile_error(N_("Must specify menu to bind key to (or \"all\")"));
		return;
	}

	auto menu = strtomenu(menu_name);
	if (menu < 1) {
		rcfile_error(N_("Could not map name \"%s\" to a menu"), menu_name.c_str());
		return;
	}

	DEBUG_LOG("unbinding \"" << key_name << "\" from menu = " << menu);

	/* Now find the apropriate entries in the menu to delete */
	for (auto s : sclist) {
		if (((s->menu & menu)) && s->keystr == key_name) {
			s->menu &= ~menu;
			DEBUG_LOG("deleted menu entry " << s->menu);
		}
	}
}

/* Read and parse additional syntax files. */
void parse_include_file(char *filename)
{
	struct stat rcinfo;

	/* Can't get the specified file's full path cause it may screw up
	our cwd depending on the parent dirs' permissions, (see Savannah bug 25297) */

	/* Don't open directories, character files, or block files. */
	if (stat(filename, &rcinfo) != -1) {
		if (S_ISDIR(rcinfo.st_mode) || S_ISCHR(rcinfo.st_mode) || S_ISBLK(rcinfo.st_mode)) {
			rcfile_error(S_ISDIR(rcinfo.st_mode) ? _("\"%s\" is a directory") : _("\"%s\" is a device file"), filename);
		}
	}

	/* Open the new syntax file. */
	std::ifstream rcstream(filename);
	if (!rcstream.is_open()) {
		rcfile_error(_("Error reading %s: %s"), filename, strerror(errno));
		return;
	}

	/* Use the name and line number position of the new syntax file
	 * while parsing it, so we can know where any errors in it are. */
	pinotrc = filename;
	lineno = 0;

	DEBUG_LOG("Parsing file \"" << filename << "\"");

	parse_rcfile(rcstream, true);
}

void parse_include(std::stringstream& line)
{
	string pinotrc_save = pinotrc;
	size_t lineno_save = lineno;
	glob_t files;

	skip_whitespace(line);
	string filename = rest(line);

	/* Expand tildes first, then the globs. */
	auto expanded = real_dir_from_tilde(filename);

	if (glob(expanded.c_str(), GLOB_ERR|GLOB_NOSORT, NULL, &files) == 0) {
		for (int i = 0; i < files.gl_pathc; ++i) {
			parse_include_file(files.gl_pathv[i]);
		}
	} else {
		rcfile_error(_("Error expanding %s: %s"), filename.c_str(), strerror(errno));
	}
	globfree(&files);

	/* We're done with the new syntax file.  Restore the original
	 * filename and line number position. */
	pinotrc = pinotrc_save;
	lineno = lineno_save;
}

/* Return the numeric value corresponding to the color named in colorname,
 * and set bright to true if that color is bright (similarly for underline). */
COLORWIDTH color_name_to_value(string colorname, bool *bright, bool *underline)
{
	COLORWIDTH mcolor = -1;

	assert(bright != NULL && underline != NULL);

	// Convert color name to lowercase so any capitalization will work
	colorname = colorname.to_lower();

	if (colorname.starts_with("bright")) {
		*bright = true;
		colorname = colorname.substr(6);
	} else if (colorname.starts_with("bold")) {
		*bright = true;
		colorname = colorname.substr(4);
	}

	if (colorname.starts_with("under")) {
		*underline = true;
		colorname = colorname.substr(5);
	}

	if (sscanf(colorname.c_str(), "%hu", &mcolor) == 1) {
		if (mcolor > 255) {
			mcolor = 255;
		}
		return mcolor;
	}

	if (colorname == "green") {
		mcolor = COLOR_GREEN;
	} else if (colorname == "red") {
		mcolor = COLOR_RED;
	} else if (colorname == "blue") {
		mcolor = COLOR_BLUE;
	} else if (colorname == "white") {
		mcolor = COLOR_WHITE;
	} else if (colorname == "yellow") {
		mcolor = COLOR_YELLOW;
	} else if (colorname == "cyan") {
		mcolor = COLOR_CYAN;
	} else if (colorname == "magenta") {
		mcolor = COLOR_MAGENTA;
	} else if (colorname == "black") {
		mcolor = COLOR_BLACK;
	} else
		rcfile_error(N_("Color \"%s\" not understood.\n"
		                "Valid colors are \"green\", \"red\", \"blue\",\n"
		                "\"white\", \"yellow\", \"cyan\", \"magenta\" and\n"
		                "\"black\", with the optional prefix \"bright\"\n"
		                "for foreground colors."), colorname.c_str());

	return mcolor;
}

/* Parse the color string in the line at ptr, and add it to the current
 * file's associated colors.  If case_senstive is true, treat the color string
 * as case sensitive. */
void parse_colors(std::stringstream& line, bool case_sensitive)
{
	COLORWIDTH fg, bg;
	bool bright = false, underline = false;

	if (!current_syntax) {
		rcfile_error(N_("Cannot add a color command without a syntax command"));
		return;
	}

	string colors;
	line >> colors;

	if (colors.empty()) {
		rcfile_error(N_("Missing color name"));
		return;
	}

	if (!parse_color_names(colors, &fg, &bg, &bright, &underline)) {
		return;
	}

	skip_whitespace(line);

	bool cancelled = false;  // The start expression was bad.
	bool expect_end = false; // Do we expect an end= line?

	// Does the next bit start with 'start='? If so, skip ahead to what's after the equal sign
	char buf[7];
	line.get(buf, 7);
	string regex;
	if (string(buf) == "start=") {
		regex = parse_regex(line);
		expect_end = true;
	} else {
		if (line.gcount() != 6) {
			line.clear();
		}
		line.seekg(-line.gcount(), line.cur);
		regex = parse_regex(line);
	}

	if (regex.empty()) {
		rcfile_error(N_("Missing regex"));
		return;
	}

	auto newcolor = ColorPtr(new colortype);

	/* Save the starting regex string if it's valid, and set up the color information. */
	if (nregcomp(regex, case_sensitive ? 0 : REG_ICASE)) {
		newcolor->fg = fg;
		newcolor->bg = bg;
		newcolor->bright = bright;
		newcolor->underline = underline;
		newcolor->icase = !case_sensitive;

		newcolor->start_regex = regex;
		newcolor->start = NULL;
		newcolor->end = NULL;

		current_syntax->add_color(newcolor);
		DEBUG_LOG("Adding new entry for fg " << fg << ", bg " << bg);
	} else {
		cancelled = true;
	}

	if (expect_end) {
		skip_whitespace(line);

		line.get(buf, 5);
		if (line.eof() || string(buf) != "end=") {
			rcfile_error(N_("\"start=\" requires a corresponding \"end=\""));
			return;
		}

		regex = parse_regex(line);

		/* If the start regex was invalid, skip past the end regex to stay in sync. */
		if (!cancelled) {
			/* Save the ending regex string if it's valid. */
			newcolor->end_regex = (nregcomp(regex, case_sensitive ? 0 : REG_ICASE)) ? regex : "";

			/* Lame way to skip another static counter */
			newcolor->id = current_syntax->nmultis;
			current_syntax->nmultis++;
		}
	}
}

/* Parse the color name, or pair of color names, in combostr. */
bool parse_color_names(const string& combostr, short *fg, short *bg, bool *bright, bool *underline)
{
	string fg_color_name, bg_color_name;

	if (combostr == "") {
		return false;
	}

	if (combostr.find(',') != string::npos) {
		fg_color_name = combostr.substr(0, combostr.find(','));
		bg_color_name = combostr.substr(combostr.find(',') + 1);

		bool bg_bright = false, bg_underline = false;
		*bg = color_name_to_value(bg_color_name, &bg_bright, &bg_underline);
		if (bg_bright) {
			rcfile_error(N_("Background color \"%s\" cannot be bright"), bg_color_name.c_str());
			return false;
		}
		if (bg_underline) {
			rcfile_error(N_("Background color \"%s\" cannot be underline"), bg_color_name.c_str());
			return false;
		}
	} else {
		fg_color_name = combostr;
		*bg = -1;
	}

	if (fg_color_name != "") {
		*fg = color_name_to_value(fg_color_name, bright, underline);

		/* Don't try to parse screwed-up foreground colors. */
		if (*fg == -1) {
			return false;
		}
	} else {
		*fg = -1;
	}

	return true;
}

/* Parse the headers (1st line) of the file which may influence the regex used. */
void parse_header(std::stringstream& line)
{
	if (!current_syntax) {
		rcfile_error(N_("Cannot add a header regex without a syntax command"));
		return;
	}

	// Everything to the end of the line is the regular expression
	string regex = parse_regex(line);

	if (regex.empty()) {
		rcfile_error(N_("Missing regex string"));
		return;
	}

	/* Save the regex string if it's valid */
	if (nregcomp(regex, 0)) {
		auto newheader = new SyntaxMatch(regex);

		DEBUG_LOG("Starting a new header entry: " << regex);

		current_syntax->headers.push_back(newheader);
	}
}

// Read in a regular expression to match the filename for the current syntax
void parse_filename_regex(std::stringstream& line)
{
	if (!current_syntax) {
		rcfile_error(N_("Cannot add a regex without a syntax command"));
		return;
	}

	// Everything after the token to the end of the line is the regex
	string regex = parse_regex(line);

	if (regex.empty()) {
		rcfile_error(N_("Missing regex"));
		return;
	}

	if (current_syntax->desc == "default") {
		rcfile_error(N_("The \"default\" syntax must take no regular expressions"));
		return;
	}

	// Try compiling the regular expression to make sure it's valid
	if (nregcomp(regex, REG_NOSUB)) {
		auto new_match = new SyntaxMatch(regex);
		current_syntax->extensions.push_back(new_match);
	}
}

/* Parse the linter requested for this syntax.  Simple? */
void parse_linter(std::stringstream& line)
{
	if (!current_syntax) {
		rcfile_error(N_("Cannot add a linter without a syntax command"));
		return;
	}
	// Everything to the end of the line is the command to execute
	string command = line.str();
	command = command.trim();

	if (command.empty()) {
		rcfile_error(N_("Missing linter command"));
		return;
	}

	current_syntax->linter = command;
}

void parse_formatter(std::stringstream& line)
{
	if (!current_syntax) {
		rcfile_error(N_("Cannot add a formatter without a syntax command"));
		return;
	}

	// Everything after the 'formatter' token on the line is the command
	string command = line.str();
	command = command.trim();

	if (command.empty()) {
		rcfile_error(N_("Missing formatter command"));
		return;
	}

	current_syntax->formatter = command;
}

/* Check whether the user has unmapped every shortcut for a
sequence we consider 'vital', like the exit function */
static void check_vitals_mapped(void)
{
	int v;
#define VITALS 5
	void (*vitals[VITALS])(void) = { do_exit, do_exit, do_cancel, do_cancel, do_cancel };
	int inmenus[VITALS] = { MMAIN, MHELP, MWHEREIS, MREPLACE, MGOTOLINE };

	for  (v = 0; v < VITALS; v++) {
		for (auto f : allfuncs) {
			if (f->scfunc == vitals[v] && f->menus & inmenus[v]) {
				const sc *s = first_sc_for(inmenus[v], f->scfunc);
				if (!s) {
					fprintf(stderr, N_("Fatal error: no keys mapped for function \"%s\""), f->desc.c_str());
					fprintf(stderr, N_("Exiting.  Please use pinot with the -I option if needed to adjust your pinotrc settings\n"));
					exit(1);
				}
				break;
			}
		}
	}
}

void parse_set(std::stringstream& line)
{
	string option;
	line >> option;
	skip_whitespace(line);

	for (auto rcopt : rcopts) {
		if (rcopt.name == option) {
			DEBUG_LOG("parse_set(): name = \"" << rcopt.name << '"');

			if (rcopt.flag != 0) {
				/* This option has a flag, so it doesn't take an argument. */
				SET(rcopt.flag);
				return;
			}

			/* This option doesn't have a flag, so it takes an argument. */

			// The argument is whatever's left over on the line
			string argument = rest(line);
			argument = argument.trim();
			if (argument.empty()) {
				rcfile_error(N_("Option \"%s\" requires an argument"), rcopt.name.c_str());
				break;
			}
			DEBUG_LOG("argument = \"" << argument << '"');

			if (rcopt.name == "titlecolor") {
				specified_color_combo[TITLE_BAR] = argument;
			} else if (rcopt.name == "statuscolor") {
				specified_color_combo[STATUS_BAR] = argument;
			} else if (rcopt.name == "keycolor") {
				specified_color_combo[KEY_COMBO] = argument;
			} else if (rcopt.name == "functioncolor") {
				specified_color_combo[FUNCTION_TAG] = argument;
			} else if (rcopt.name == "fill") {
				if (!parse_num(argument.c_str(), &wrap_at)) {
					rcfile_error(N_("Requested fill size \"%s\" is invalid"), argument.c_str());
					wrap_at = -CHARS_FROM_EOL;
				}
			} else if (rcopt.name == "matchbrackets") {
				if (argument.has_blank_chars()) {
					rcfile_error(N_("Non-blank characters required"));
				} else {
					matchbrackets = argument;
				}
			} else if (rcopt.name == "whitespace") {
				whitespace = argument;
				if (mbstrlen(whitespace.c_str()) != 2 || strlenpt(whitespace.c_str()) != 2) {
					rcfile_error(N_("Two single-column characters required"));
					whitespace = "";
				} else {
					whitespace_len[0] = parse_mbchar(whitespace.c_str(), NULL, NULL);
					whitespace_len[1] = parse_mbchar(whitespace.c_str() + whitespace_len[0], NULL, NULL);
				}
			} else if (rcopt.name == "backupdir") {
				backup_dir = argument;
			} else if (rcopt.name == "speller") {
				alt_speller = argument;
			} else if (rcopt.name == "tabsize") {
				if (!parse_num(argument.c_str(), &tabsize) || tabsize <= 0) {
					rcfile_error(N_("Requested tab size \"%s\" is invalid"), argument.c_str());
					tabsize = -1;
				}
			}
			return;
		}
	}

	rcfile_error(N_("Unknown flag \"%s\""), option.c_str());
}

void parse_unset(std::stringstream& line)
{
	string option;
	line >> option;

	for (auto rcopt : rcopts) {
		if (rcopt.name == option) {
			DEBUG_LOG("parse_unset(): name = \"" << rcopt.name << '"');

			if (rcopt.flag != 0) {
				UNSET(rcopt.flag);
			} else {
				rcfile_error(N_("Cannot unset flag \"%s\""), rcopt.name.c_str());
			}
			return;
		}
	}

	rcfile_error(N_("Unknown flag \"%s\""), option.c_str());
}

/* Parse the rcfile, once it has been opened successfully at rcstream,
 * and close it afterwards.  If syntax_only is true, only allow the file
 * to contain color syntax commands: syntax, color, and icolor. */
void parse_rcfile(std::ifstream& rcstream, bool syntax_only)
{
	string line;

	while (!getline(rcstream, line).eof()) {
		lineno++;

		// Skip empty lines and lines that begin with '#' (comments)
		if (line.empty() || line[0] == '#') {
			continue;
		}

		std::stringstream linestream;
		linestream << line;

		// Read keyword
		string keyword;
		linestream >> keyword;
		skip_whitespace(linestream);

		if (keyword == "set") {
			if (syntax_only) {
				rcfile_error(N_("Command \"%s\" not allowed in included file"), keyword.c_str());
			} else {
				parse_set(linestream);
			}
		} else if (keyword == "unset") {
			if (syntax_only) {
				rcfile_error(N_("Command \"%s\" not allowed in included file"), keyword.c_str());
			} else {
				parse_unset(linestream);
			}
		} else if (keyword == "include") {
			if (syntax_only) {
				rcfile_error(N_("Command \"%s\" not allowed in included file"), keyword.c_str());
			} else {
				parse_include(linestream);
			}
		} else if (keyword == "syntax") {
			parse_syntax(linestream);
		} else if (keyword == "extends") {
			parse_extends(linestream);
		} else if (keyword == "filename") {
			parse_filename_regex(linestream);
		} else if (keyword == "magic") {
#ifdef HAVE_LIBMAGIC
			parse_magic(linestream);
#endif
		} else if (keyword == "header") {
			parse_header(linestream);
		} else if (keyword == "color") {
			parse_colors(linestream, true);
		} else if (keyword == "icolor") {
			parse_colors(linestream, false);
		} else if (keyword == "bind") {
			parse_keybinding(linestream);
		} else if (keyword == "unbind") {
			parse_unbinding(linestream);
		} else if (keyword == "linter") {
			parse_linter(linestream);
		} else if (keyword == "formatter") {
			parse_formatter(linestream);
		} else {
			rcfile_error(N_("Command \"%s\" not understood"), keyword.c_str());
		}
	}

	if (current_syntax != NULL && current_syntax->own_colors().empty()) {
		rcfile_error(N_("Syntax \"%s\" has no color commands"), current_syntax->desc.c_str());
	}

	lineno = 0;

	check_vitals_mapped();
	return;
}

/* The main rcfile function.  It tries to open the system-wide rcfile,
 * followed by the current user's rcfile. */
void do_rcfile(void)
{
	struct stat rcinfo;

	pinotrc = string(SYSCONFDIR) + "/pinotrc";

	/* Don't open directories, character files, or block files. */
	if (stat(pinotrc, &rcinfo) != -1) {
		if (S_ISDIR(rcinfo.st_mode) || S_ISCHR(rcinfo.st_mode) || S_ISBLK(rcinfo.st_mode)) {
			rcfile_error(S_ISDIR(rcinfo.st_mode) ? _("\"%s\" is a directory") : _("\"%s\" is a device file"), pinotrc.c_str());
		}
	}

	DEBUG_LOG("Parsing file \"" << pinotrc << '"');

	/* Try to open the system-wide pinotrc. */
	std::ifstream rcstream(pinotrc.c_str());
	if (rcstream.is_open()) {
		parse_rcfile(rcstream, false);
	}

#ifdef DISABLE_ROOTWRAPPING
	/* We've already read SYSCONFDIR/pinotrc, if it's there.  If we're
	 * root, and --disable-wrapping-as-root is used, turn wrapping off
	 * now. */
	if (geteuid() == PINOT_ROOT_UID) {
		SET(NO_WRAP);
	}
#endif

	get_homedir();

	if (homedir == "") {
		rcfile_error(N_("I can't find my home directory!  Wah!"));
	} else {
#ifndef RCFILE_NAME
#define RCFILE_NAME ".pinotrc"
#endif
		pinotrc = homedir + "/" + RCFILE_NAME;

		/* Don't open directories, character files, or block files. */
		if (stat(pinotrc, &rcinfo) != -1) {
			if (S_ISDIR(rcinfo.st_mode) || S_ISCHR(rcinfo.st_mode) || S_ISBLK(rcinfo.st_mode)) {
				rcfile_error(S_ISDIR(rcinfo.st_mode) ? _("\"%s\" is a directory") : _("\"%s\" is a device file"), pinotrc.c_str());
			}
		}

		/* Try to open the current user's pinotrc. */
		rcstream.open(pinotrc.c_str());
		if (!rcstream.is_open()) {
			/* Don't complain about the file's not existing. */
			if (errno != ENOENT) {
				rcfile_error(N_("Error reading %s: %s"), pinotrc.c_str(), strerror(errno));
			}
		} else {
			parse_rcfile(rcstream, false);
		}
	}

	pinotrc = "";

	if (errors && !ISSET(QUIET)) {
		errors = false;
		fprintf(stderr, _("\nPress Enter to continue starting pinot.\n"));
		while (getchar() != '\n') {
			;
		}
	}
}
