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
static std::string pinotrc;
/* The path to the rcfile we're parsing. */
static Syntax *new_syntax = NULL;
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

/* Parse the next word from the string, null-terminate it, and return
 * a pointer to the first character after the null terminator.  The
 * returned pointer will point to '\0' if we hit the end of the line. */
char *parse_next_word(char *ptr)
{
	while (!isblank(*ptr) && *ptr != '\0') {
		ptr++;
	}

	if (*ptr == '\0') {
		return ptr;
	}

	/* Null-terminate and advance ptr. */
	*ptr++ = '\0';

	while (isblank(*ptr)) {
		ptr++;
	}

	return ptr;
}

/* Parse an argument, with optional quotes, after a keyword that takes
 * one.  If the next word starts with a ", we say that it ends with the
 * last " of the line.  Otherwise, we interpret it as usual, so that the
 * arguments can contain "'s too. */
char *parse_argument(char *ptr)
{
	const char *ptr_save = ptr;
	char *last_quote = NULL;

	assert(ptr != NULL);

	if (*ptr != '"') {
		return parse_next_word(ptr);
	}

	do {
		ptr++;
		if (*ptr == '"') {
			last_quote = ptr;
		}
	} while (*ptr != '\0');

	if (last_quote == NULL) {
		if (*ptr == '\0') {
			ptr = NULL;
		} else {
			*ptr++ = '\0';
		}
		rcfile_error(N_("Argument '%s' has an unterminated \""), ptr_save);
	} else {
		*last_quote = '\0';
		ptr = last_quote + 1;
	}
	if (ptr != NULL)
		while (isblank(*ptr)) {
			ptr++;
		}
	return ptr;
}

/* Parse the next regex string from the line at ptr, and return it. */
char *parse_next_regex(char *ptr)
{
	assert(ptr != NULL);

	/* Continue until the end of the line, or a " followed by a space, a
	 * blank character, or \0. */
	while ((*ptr != '"' || (!isblank(*(ptr + 1)) && *(ptr + 1) != '\0')) && *ptr != '\0') {
		ptr++;
	}

	assert(*ptr == '"' || *ptr == '\0');

	if (*ptr == '\0') {
		rcfile_error(N_("Regex strings must begin and end with a \" character"));
		return NULL;
	}

	/* Null-terminate and advance ptr. */
	*ptr++ = '\0';

	while (isblank(*ptr)) {
		ptr++;
	}

	return ptr;
}

/* Compile the regular expression regex to see if it's valid.  Return
 * true if it is, or false otherwise. */
bool nregcomp(const char *regex, int cflags)
{
	regex_t preg;
	int rc = regcomp(&preg, regex, REG_EXTENDED | cflags);

	if (rc != 0) {
		size_t len = regerror(rc, &preg, NULL, 0);
		char *str = charalloc(len);

		regerror(rc, &preg, str, len);
		rcfile_error(N_("Bad regex \"%s\": %s"), regex, str);
		free(str);
	}

	regfree(&preg);
	return (rc == 0);
}

/* Parse the next syntax string from the line at ptr, and add it to the
 * global list of color syntaxes. */
void parse_syntax(char *ptr)
{
	const char *fileregptr = NULL, *nameptr = NULL;

	assert(ptr != NULL);

	if (*ptr == '\0') {
		rcfile_error(N_("Missing syntax name"));
		return;
	}

	if (*ptr != '"') {
		rcfile_error(N_("Regex strings must begin and end with a \" character"));
		return;
	}

	ptr++;

	nameptr = ptr;
	ptr = parse_next_regex(ptr);

	if (ptr == NULL) {
		return;
	}

	/* Search for a duplicate syntax name.  If we find one, free it, so
	 * that we always use the last syntax with a given name. */
	auto existing_syntax = syntaxes[nameptr];
	if (existing_syntax) {
		DEBUG_LOG("Found existing syntax with name \"" << nameptr << "\"; overriding with new definition");
		delete existing_syntax;
		existing_syntax = nullptr;
	}

	new_syntax = new Syntax(nameptr);

	DEBUG_LOG("Starting a new syntax type: \"" << nameptr << '"');

	/* The "none" syntax is the same as not having a syntax at all, so
	 * we can't assign any extensions or colors to it. */
	if (new_syntax->desc == "none") {
		rcfile_error(N_("The \"none\" syntax is reserved"));
		return;
	}

	/* The default syntax should have no associated extensions. */
	if (new_syntax->desc == "default" && *ptr != '\0') {
		rcfile_error(N_("The \"default\" syntax must take no extensions"));
		return;
	}

	/* Now load the extensions into their part of the struct. */
	while (*ptr != '\0') {
		while (*ptr != '"' && *ptr != '\0') {
			ptr++;
		}

		if (*ptr == '\0') {
			return;
		}

		ptr++;

		fileregptr = ptr;
		ptr = parse_next_regex(ptr);
		if (ptr == NULL) {
			break;
		}

		/* Save the extension regex if it's valid. */
		if (nregcomp(fileregptr, REG_NOSUB)) {
			auto newext = new SyntaxMatch(fileregptr);
			new_syntax->extensions.push_back(newext);
		}
	}

	syntaxes[nameptr] = new_syntax;
}

/* Parse an optional "extends" line in a syntax. */
void parse_extends(char *ptr)
{
	assert(ptr != NULL);

	if (syntaxes.empty()) {
		rcfile_error(N_("Cannot use 'extends' command without a 'syntax' command"));
		return;
	}

	if (*ptr == '\0') {
		rcfile_error(N_("Missing name of syntax to extend"));
	}

	if (*ptr != '"') {
		rcfile_error(N_("'extend' names must begin and end with a \" character"));
		return;
	}

	char *syntax_name = ptr;
	if (*syntax_name == '"') {
		syntax_name++;
	}
	ptr = parse_argument(ptr);

	DEBUG_LOG("Reading a new 'extends': \"" << syntax_name << '"');

	new_syntax->extends.push_back(syntax_name);
}

#ifdef HAVE_LIBMAGIC
/* Parse the next syntax string from the line at ptr, and add it to the
 * global list of color syntaxes. */
void parse_magictype(char *ptr)
{
	const char *fileregptr = NULL;

	assert(ptr != NULL);

	if (syntaxes.empty()) {
		rcfile_error(N_("Cannot add a magic string regex without a syntax command"));
		return;
	}

	if (*ptr == '\0') {
		rcfile_error(N_("Missing magic string name"));
		return;
	}

	if (*ptr != '"') {
		rcfile_error(N_("Regex strings must begin and end with a \" character"));
		return;
	}

	DEBUG_LOG("Starting a magic type: \"" << ptr << '"');

	/* Now load the extensions into their part of the struct. */
	while (*ptr != '\0') {
		while (*ptr != '"' && *ptr != '\0') {
			ptr++;
		}

		if (*ptr == '\0') {
			return;
		}

		ptr++;

		fileregptr = ptr;
		ptr = parse_next_regex(ptr);
		if (ptr == NULL) {
			break;
		}

		/* Save the regex if it's valid. */
		if (nregcomp(fileregptr, REG_NOSUB)) {
			auto newext = new SyntaxMatch(fileregptr);
			new_syntax->magics.push_back(newext);
		}
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

void parse_keybinding(char *ptr)
{
	char *keyptr = NULL, *keycopy = NULL, *funcptr = NULL, *menuptr = NULL;
	sc *newsc;
	int menu;

	assert(ptr != NULL);

	if (*ptr == '\0') {
		rcfile_error(N_("Missing key name"));
		return;
	}

	keyptr = ptr;
	ptr = parse_next_word(ptr);
	keycopy = mallocstrcpy(NULL, keyptr);

	if (keycopy[0] != 'M' && keycopy[0] != '^' && keycopy[0] != 'F') {
		rcfile_error(N_("keybindings must begin with \"^\", \"M\", or \"F\""));
		return;
	}

	funcptr = ptr;
	ptr = parse_next_word(ptr);

	if (!strcmp(funcptr, "")) {
		rcfile_error(N_("Must specify function to bind key to"));
		return;
	}

	menuptr = ptr;
	ptr = parse_next_word(ptr);

	if (!strcmp(menuptr, "")) {
		/* Note to translators, do not translate the word "all"
		   in the sentence below, everything else is fine */
		rcfile_error(N_("Must specify menu to bind key to (or \"all\")"));
		return;
	}

	menu = strtomenu(menuptr);
	newsc = strtosc(funcptr);
	if (newsc == NULL) {
		rcfile_error(N_("Could not map name \"%s\" to a function"), funcptr);
		return;
	}

	if (menu < 1) {
		rcfile_error(N_("Could not map name \"%s\" to a menu"), menuptr);
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
		rcfile_error(N_("Function '%s' does not exist in menu '%s'"), funcptr, menuptr);
		free(newsc);
		return;
	}

	newsc->keystr = keycopy;
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
void parse_unbinding(char *ptr)
{
	char *keyptr = NULL, *keycopy = NULL, *menuptr = NULL;
	int menu;

	assert(ptr != NULL);

	if (*ptr == '\0') {
		rcfile_error(N_("Missing key name"));
		return;
	}

	keyptr = ptr;
	ptr = parse_next_word(ptr);
	keycopy = mallocstrcpy(NULL, keyptr);

	DEBUG_LOG("Starting unbinding code");

	if (keycopy[0] != 'M' && keycopy[0] != '^' && keycopy[0] != 'F' && keycopy[0] != 'K') {
		rcfile_error(N_("keybindings must begin with \"^\", \"M\", or \"F\""));
		return;
	}

	menuptr = ptr;
	ptr = parse_next_word(ptr);

	if (!strcmp(menuptr, "")) {
		/* Note to translators, do not translate the word "all"
		   in the sentence below, everything else is fine */
		rcfile_error(N_("Must specify menu to bind key to (or \"all\")"));
		return;
	}

	menu = strtomenu(menuptr);
	if (menu < 1) {
		rcfile_error(N_("Could not map name \"%s\" to a menu"), menuptr);
		return;
	}

	DEBUG_LOG("unbinding \"" << keycopy << "\" from menu = " << menu);

	/* Now find the apropriate entries in the menu to delete */
	for (auto s : sclist) {
		if (((s->menu & menu)) && s->keystr == keycopy) {
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

void parse_include(char *ptr)
{
	char *option, *expanded;
	std::string pinotrc_save = pinotrc;
	size_t lineno_save = lineno;
	glob_t files;

	option = ptr;
	if (*option == '"') {
		option++;
	}
	ptr = parse_argument(ptr);

	/* Expand tildes first, then the globs. */
	expanded = real_dir_from_tilde(option);

	if (glob(expanded, GLOB_ERR|GLOB_NOSORT, NULL, &files) == 0) {
		for (int i = 0; i < files.gl_pathc; ++i) {
			parse_include_file(files.gl_pathv[i]);
		}
	} else {
		rcfile_error(_("Error expanding %s: %s"), option, strerror(errno));
	}
	globfree(&files);
	free(expanded);

	/* We're done with the new syntax file.  Restore the original
	 * filename and line number position. */
	pinotrc = pinotrc_save;
	lineno = lineno_save;
}

/* Return the numeric value corresponding to the color named in colorname,
 * and set bright to true if that color is bright (similarly for underline). */
COLORWIDTH color_name_to_value(std::string colorname, bool *bright, bool *underline)
{
	COLORWIDTH mcolor = -1;

	assert(bright != NULL && underline != NULL);

	// Convert color name to lowercase so any capitalization will work
	std::transform(colorname.begin(), colorname.end(), colorname.begin(), ::tolower);

	if (colorname.compare(0, 6, "bright") == 0) {
		*bright = true;
		colorname = colorname.substr(6);
	} else if (colorname.compare(0, 4, "bold") == 0) {
		*bright = true;
		colorname = colorname.substr(4);
	}

	if (colorname.compare(0, 5, "under") == 0) {
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
 * file's associated colors.  If icase is true, treat the color string
 * as case insensitive. */
void parse_colors(char *ptr, bool icase)
{
	COLORWIDTH fg, bg;
	bool bright = false, underline = false;
	char *fgstr;

	assert(ptr != NULL);

	if (syntaxes.empty()) {
		rcfile_error(N_("Cannot add a color command without a syntax command"));
		return;
	}

	if (*ptr == '\0') {
		rcfile_error(N_("Missing color name"));
		return;
	}

	fgstr = ptr;
	ptr = parse_next_word(ptr);

	if (!parse_color_names(std::string(fgstr), &fg, &bg, &bright, &underline)) {
		return;
	}

	if (*ptr == '\0') {
		rcfile_error(N_("Missing regex string"));
		return;
	}

	/* Now for the fun part.  Start adding regexes to individual strings
	 * in the colorstrings array, woo! */
	while (ptr != NULL && *ptr != '\0') {
		/* The new color structure. */
		bool cancelled = false;
		/* The start expression was bad. */
		bool expectend = false;
		/* Do we expect an end= line? */

		if (strncasecmp(ptr, "start=", 6) == 0) {
			ptr += 6;
			expectend = true;
		}

		if (*ptr != '"') {
			rcfile_error(N_("Regex strings must begin and end with a \" character"));
			ptr = parse_next_regex(ptr);
			continue;
		}

		ptr++;

		fgstr = ptr;
		ptr = parse_next_regex(ptr);
		if (ptr == NULL) {
			break;
		}

		auto newcolor = ColorPtr(new colortype);

		/* Save the starting regex string if it's valid, and set up the color information. */
		if (nregcomp(fgstr, icase ? REG_ICASE : 0)) {
			newcolor->fg = fg;
			newcolor->bg = bg;
			newcolor->bright = bright;
			newcolor->underline = underline;
			newcolor->icase = icase;

			newcolor->start_regex = mallocstrcpy(NULL, fgstr);
			newcolor->start = NULL;

			newcolor->end_regex = NULL;
			newcolor->end = NULL;

			new_syntax->add_color(newcolor);
#ifdef DEBUG
			if (new_syntax->own_colors().empty()) {
				DEBUG_LOG("Starting a new colorstring for fg " << fg << ", bg " << bg);
			} else {
				DEBUG_LOG("Adding new entry for fg " << fg << ", bg " << bg);
			}
#endif
		} else {
			cancelled = true;
		}

		if (expectend) {
			if (ptr == NULL || strncasecmp(ptr, "end=", 4) != 0) {
				rcfile_error(N_("\"start=\" requires a corresponding \"end=\""));
				return;
			}
			ptr += 4;
			if (*ptr != '"') {
				rcfile_error(N_("Regex strings must begin and end with a \" character"));
				continue;
			}

			ptr++;

			fgstr = ptr;
			ptr = parse_next_regex(ptr);
			if (ptr == NULL) {
				break;
			}

			/* If the start regex was invalid, skip past the end regex to stay in sync. */
			if (cancelled) {
				continue;
			}

			/* Save the ending regex string if it's valid. */
			newcolor->end_regex = (nregcomp(fgstr, icase ? REG_ICASE : 0)) ? mallocstrcpy(NULL, fgstr) : NULL;

			/* Lame way to skip another static counter */
			newcolor->id = new_syntax->nmultis;
			new_syntax->nmultis++;
		}
	}
}

/* Parse the color name, or pair of color names, in combostr. */
bool parse_color_names(const std::string& combostr, short *fg, short *bg, bool *bright, bool *underline)
{
	std::string fg_color_name, bg_color_name;

	if (combostr == "") {
		return false;
	}

	if (combostr.find(',') != std::string::npos) {
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
void parse_headers(char *ptr)
{
	char *regstr;

	assert(ptr != NULL);

	if (syntaxes.empty()) {
		rcfile_error(N_("Cannot add a header regex without a syntax command"));
		return;
	}

	if (*ptr == '\0') {
		rcfile_error(N_("Missing regex string"));
		return;
	}

	/* Now for the fun part.  Start adding regexes to individual strings
	 * in the colorstrings array, woo! */
	while (ptr != NULL && *ptr != '\0') {
		if (*ptr != '"') {
			rcfile_error(N_("Regex strings must begin and end with a \" character"));
			ptr = parse_next_regex(ptr);
			continue;
		}

		ptr++;

		regstr = ptr;
		ptr = parse_next_regex(ptr);
		if (ptr == NULL) {
			break;
		}


		/* Save the regex string if it's valid */
		if (nregcomp(regstr, 0)) {
			auto newheader = new SyntaxMatch(regstr);

			DEBUG_LOG("Starting a new header entry: " << regstr);

			new_syntax->headers.push_back(newheader);
		}
	}
}

/* Parse the linter requested for this syntax.  Simple? */
void parse_linter(char *ptr)
{
	assert(ptr != NULL);

	if (syntaxes.empty()) {
		rcfile_error(N_("Cannot add a linter without a syntax command"));
		return;
	}

	if (*ptr == '\0') {
		rcfile_error(N_("Missing linter command"));
		return;
	}

	new_syntax->linter = std::string(ptr);
}

void parse_formatter(char *ptr)
{
	assert(ptr != NULL);

	if (syntaxes.empty()) {
		rcfile_error(N_("Cannot add a formatter without a syntax command"));
		return;
	}

	if (*ptr == '\0') {
		rcfile_error(N_("Missing formatter command"));
		return;
	}

	new_syntax->formatter = std::string(ptr);
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

std::string rest(std::stringstream& stream)
{
	// Eliminate leading whitespace
	while (!stream.eof() && isspace(stream.peek())) stream.get();

	std::string remaining = stream.eof() ? "" : stream.str().substr(stream.tellg());
	return remaining;
}

/* Parse the rcfile, once it has been opened successfully at rcstream,
 * and close it afterwards.  If syntax_only is true, only allow the file
 * to contain color syntax commands: syntax, color, and icolor. */
void parse_rcfile(std::ifstream &rcstream, bool syntax_only)
{
	std::string line;

	while (!std::getline(rcstream, line).eof()) {
		lineno++;

		if (line.empty() || line[0] == '#') {
			continue;
		}

		std::stringstream linestream;
		linestream << line;

		// Read keyword
		std::string keyword;
		linestream >> keyword;

		// --------

		char *ptr = NULL;
		int set = 0;

		/* Try to parse the keyword. */
		if (keyword == "set") {
			if (syntax_only) {
				rcfile_error(N_("Command \"%s\" not allowed in included file"), keyword.c_str());
			} else {
				set = 1;
			}
		} else if (keyword == "unset") {
			if (syntax_only) {
				rcfile_error(N_("Command \"%s\" not allowed in included file"), keyword.c_str());
			} else {
				set = -1;
			}
		}	else if (keyword == "include") {
			if (syntax_only) {
				rcfile_error(N_("Command \"%s\" not allowed in included file"), keyword.c_str());
			} else {
				ptr = mallocstrcpy(ptr, rest(linestream).c_str());
				parse_include(ptr);
				free(ptr);
			}
		} else if (keyword == "syntax") {
			if (new_syntax != NULL && new_syntax->own_colors().empty()) {
				rcfile_error(N_("Syntax \"%s\" has no color commands"), new_syntax->desc.c_str());
			}
			ptr = mallocstrcpy(ptr, rest(linestream).c_str());
			parse_syntax(ptr);
			free(ptr);
		} else if (keyword == "extends") {
			ptr = mallocstrcpy(ptr, rest(linestream).c_str());
			parse_extends(ptr);
			free(ptr);
		} else if (keyword == "magic") {
#ifdef HAVE_LIBMAGIC
			ptr = mallocstrcpy(ptr, rest(linestream).c_str());
			parse_magictype(ptr);
			free(ptr);
#endif
		} else if (keyword == "header") {
			ptr = mallocstrcpy(ptr, rest(linestream).c_str());
			parse_headers(ptr);
			free(ptr);
		} else if (keyword == "color") {
			ptr = mallocstrcpy(ptr, rest(linestream).c_str());
			parse_colors(ptr, false);
			free(ptr);
		} else if (keyword == "icolor") {
			ptr = mallocstrcpy(ptr, rest(linestream).c_str());
			parse_colors(ptr, true);
			free(ptr);
		} else if (keyword == "bind") {
			ptr = mallocstrcpy(ptr, rest(linestream).c_str());
			parse_keybinding(ptr);
			free(ptr);
		} else if (keyword == "unbind") {
			ptr = mallocstrcpy(ptr, rest(linestream).c_str());
			parse_unbinding(ptr);
			free(ptr);
		} else if (keyword == "linter") {
			ptr = mallocstrcpy(ptr, rest(linestream).c_str());
			parse_linter(ptr);
			free(ptr);
		} else if (keyword == "formatter") {
			ptr = mallocstrcpy(ptr, rest(linestream).c_str());
			parse_formatter(ptr);
			free(ptr);
		} else {
			rcfile_error(N_("Command \"%s\" not understood"), keyword.c_str());
		}

		if (set == 0) {
			continue;
		}

		std::string option;
		linestream >> option;

		bool found = false;
		for (auto rcopt : rcopts) {
			if (rcopt.name == option) {
				found = true;
				DEBUG_LOG("parse_rcfile(): name = \"" << rcopt.name << '"');
				if (set == 1) {
					if (rcopt.flag != 0) {
						/* This option has a flag, so it doesn't take an argument. */
						SET(rcopt.flag);
					} else {
						/* This option doesn't have a flag, so it takes an argument. */
						ptr = mallocstrcpy(ptr, rest(linestream).c_str());
						char *value = ptr;
						// Skip leading '"' character at front of string
						if (*value == '"') value++;
						ptr = parse_argument(ptr);

						std::string argument = std::string(value);
						if (argument.empty()) {
							rcfile_error(N_("Option \"%s\" requires an argument"), rcopt.name.c_str());
							break;
						}
						DEBUG_LOG("argument = \"" << argument << '"');

						/* Make sure option is a valid multibyte string. */
						if (!is_valid_mbstring(argument.c_str())) {
							rcfile_error(N_("Option is not a valid multibyte string"));
							break;
						}

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
							matchbrackets = mallocstrcpy(matchbrackets, argument.c_str());
							if (has_blank_mbchars(matchbrackets)) {
								rcfile_error(N_("Non-blank characters required"));
								matchbrackets = NULL;
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
							alt_speller = mallocstrcpy(alt_speller, argument.c_str());
						} else if (rcopt.name == "tabsize") {
							if (!parse_num(argument.c_str(), &tabsize) || tabsize <= 0) {
								rcfile_error(N_("Requested tab size \"%s\" is invalid"), argument.c_str());
								tabsize = -1;
							}
						} else {
							assert(false);
						}
					}
					DEBUG_LOG("flag = " << rcopt.flag);
				} else if (rcopt.flag != 0) {
					UNSET(rcopt.flag);
				} else {
					rcfile_error(N_("Cannot unset flag \"%s\""), rcopt.name.c_str());
				}
				break;
			}
		}
		if (!found) {
			rcfile_error(N_("Unknown flag \"%s\""), option.c_str());
		}
	}

	if (new_syntax != NULL && new_syntax->own_colors().empty()) {
		rcfile_error(N_("Syntax \"%s\" has no color commands"), new_syntax->desc.c_str());
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

	pinotrc = std::string(SYSCONFDIR) + "/pinotrc";

	/* Don't open directories, character files, or block files. */
	if (stat(pinotrc, &rcinfo) != -1) {
		if (S_ISDIR(rcinfo.st_mode) || S_ISCHR(rcinfo.st_mode) || S_ISBLK(rcinfo.st_mode)) {
			rcfile_error(S_ISDIR(rcinfo.st_mode) ? _("\"%s\" is a directory") : _("\"%s\" is a device file"), pinotrc.c_str());
		}
	}

	DEBUG_LOG("Parsing file \"" << pinotrc << '"');

	/* Try to open the system-wide pinotrc. */
	std::ifstream rcstream(pinotrc);
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
		rcstream.open(pinotrc);
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
