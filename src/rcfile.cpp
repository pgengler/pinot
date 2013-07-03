/* $Id: rcfile.c 4530 2011-02-18 07:30:57Z astyanax $ */
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

#include <fstream>
#include <vector>

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#ifdef ENABLE_PINOTRC

std::vector<rcoption> rcopts = {
	{"boldtext", BOLD_TEXT, false},
#ifdef ENABLE_JUSTIFY
	{"brackets", 0, false},
#endif
	{"const", CONST_UPDATE, false},
#ifndef DISABLE_WRAPJUSTIFY
	{"fill", 0, true},
#endif
	{"locking", LOCKING, false},
#ifndef DISABLE_MOUSE
	{"mouse", USE_MOUSE, false},
#endif
#ifdef ENABLE_MULTIBUFFER
	{"multibuffer", MULTIBUFFER, false},
#endif
	{"morespace", MORE_SPACE, false},
	{"nofollow", NOFOLLOW_SYMLINKS, false},
	{"nohelp", NO_HELP, false},
	{"nonewlines", NO_NEWLINES, false},
#ifndef DISABLE_WRAPPING
	{"nowrap", NO_WRAP, false},
#endif
#ifndef DISABLE_OPERATINGDIR
	{"operatingdir", 0, false},
#endif
	{"preserve", PRESERVE, false},
#ifdef ENABLE_JUSTIFY
	{"punct", 0, false},
	{"quotestr", 0, false},
#endif
	{"rebinddelete", REBIND_DELETE, false},
	{"rebindkeypad", REBIND_KEYPAD, false},
#ifdef HAVE_PCREPOSIX_H
	{"regexp", USE_REGEXP, false},
#endif
#ifdef ENABLE_SPELLER
	{"speller", 0, false},
#endif
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
	{"undo", UNDOABLE, false},
	{"whitespace", 0, false},
	{"wordbounds", WORD_BOUNDS, false},
	{"softwrap", SOFTWRAP, true},
};

static bool errors = false;
/* Whether we got any errors while parsing an rcfile. */
static size_t lineno = 0;
/* If we did, the line number where the last error occurred. */
static char *pinotrc = NULL;
/* The path to the rcfile we're parsing. */
#ifdef ENABLE_COLOR
static Syntax *new_syntax = NULL;
/* current syntax being processed */
#endif

/* We have an error in some part of the rcfile.  Print the error message
 * on stderr, and then make the user hit Enter to continue starting
 * pinot. */
void rcfile_error(const char *msg, ...)
{
	va_list ap;

	if (ISSET(QUIET)) {
		return;
	}

	fprintf(stderr, "\n");
	if (lineno > 0) {
		errors = true;
		fprintf(stderr, _("Error in %s on line %lu: "), pinotrc, (unsigned long)lineno);
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

#ifdef ENABLE_COLOR
/* Parse the next regex string from the line at ptr, and return it. */
char *parse_next_regex(char *ptr)
{
	assert(ptr != NULL);

	/* Continue until the end of the line, or a " followed by a space, a
	 * blank character, or \0. */
	while ((*ptr != '"' || (!isblank(*(ptr + 1)) &&
	                        *(ptr + 1) != '\0')) && *ptr != '\0') {
		ptr++;
	}

	assert(*ptr == '"' || *ptr == '\0');

	if (*ptr == '\0') {
		rcfile_error(
		    N_("Regex strings must begin and end with a \" character"));
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
	const char *r = fixbounds(regex);
	int rc = regcomp(&preg, r, REG_EXTENDED | cflags);

	if (rc != 0) {
		size_t len = regerror(rc, &preg, NULL, 0);
		char *str = charalloc(len);

		regerror(rc, &preg, str, len);
		rcfile_error(N_("Bad regex \"%s\": %s"), r, str);
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
		DEBUG_LOG("Found existing syntax with name \"%s\"; overriding with new definition\n", nameptr);
		delete existing_syntax;
		existing_syntax = nullptr;
	}

	new_syntax = new Syntax;
	new_syntax->desc = std::string(nameptr);
	new_syntax->nmultis = 0;

	DEBUG_LOG("Starting a new syntax type: \"%s\"\n", nameptr);

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

	DEBUG_LOG("Reading a new 'extends': \"%s\"\n", syntax_name);

	new_syntax->extends.push_back(syntax_name);
}

/* Parse the next syntax string from the line at ptr, and add it to the
 * global list of color syntaxes. */
void parse_magictype(char *ptr)
{
#ifdef HAVE_LIBMAGIC
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

	DEBUG_LOG("Starting a magic type: \"%s\"\n", ptr);

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
#endif /* HAVE_LIBMAGIC */
}

int check_bad_binding(sc *s)
{
#define BADLISTLEN 1
	int badtypes[BADLISTLEN] = {META};
	int badseqs[BADLISTLEN] = { 91 };
	int i;

	for (i = 0; i < BADLISTLEN; i++)
		if (s->type == badtypes[i] && s->seq == badseqs[i]) {
			return 1;
		}

	return 0;
}

void parse_keybinding(char *ptr)
{
	char *keyptr = NULL, *keycopy = NULL, *funcptr = NULL, *menuptr = NULL;
	sc *s, *newsc;
	int i, menu;

	assert(ptr != NULL);

	if (*ptr == '\0') {
		rcfile_error(N_("Missing key name"));
		return;
	}

	keyptr = ptr;
	ptr = parse_next_word(ptr);
	keycopy = mallocstrcpy(NULL, keyptr);
	for (i = 0; i < strlen(keycopy); i++) {
		keycopy[i] = toupper(keycopy[i]);
	}

	if (keycopy[0] != 'M' && keycopy[0] != '^' && keycopy[0] != 'F' && keycopy[0] != 'K') {
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
	newsc = strtosc(menu, funcptr);
	if (newsc == NULL) {
		rcfile_error(N_("Could not map name \"%s\" to a function"), funcptr);
		return;
	}

	if (menu < 1) {
		rcfile_error(N_("Could not map name \"%s\" to a menu"), menuptr);
		return;
	}


	DEBUG_LOG("newsc now address %d, menu func assigned = %d, menu = %d\n", &newsc, newsc->scfunc, menu);


	newsc->keystr = keycopy;
	newsc->menu = menu;
	newsc->type = strtokeytype(newsc->keystr);
	assign_keyinfo(newsc);
	DEBUG_LOG("s->keystr = \"%s\"\n", newsc->keystr);
	DEBUG_LOG("s->seq = \"%d\"\n", newsc->seq);

	if (check_bad_binding(newsc)) {
		rcfile_error(N_("Sorry, keystr \"%s\" is an illegal binding"), newsc->keystr);
		return;
	}

	/* now let's have some fun.  Try and delete the other entries
	   we found for the same menu, then make this new new
	   beginning */
	for (s = sclist; s != NULL; s = s->next) {
		if (((s->menu & newsc->menu)) && s->seq == newsc->seq) {
			s->menu &= ~newsc->menu;
			DEBUG_LOG("replaced menu entry %d\n", s->menu);
		}
	}
	newsc->next = sclist;
	sclist = newsc;
}

/* Let user unbind a sequence from a given (or all) menus */
void parse_unbinding(char *ptr)
{
	char *keyptr = NULL, *keycopy = NULL, *menuptr = NULL;
	sc *s;
	int i, menu;

	assert(ptr != NULL);

	if (*ptr == '\0') {
		rcfile_error(N_("Missing key name"));
		return;
	}

	keyptr = ptr;
	ptr = parse_next_word(ptr);
	keycopy = mallocstrcpy(NULL, keyptr);
	for (i = 0; i < strlen(keycopy); i++) {
		keycopy[i] = toupper(keycopy[i]);
	}

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


	DEBUG_LOG("unbinding \"%s\" from menu = %d\n", keycopy, menu);

	/* Now find the apropriate entries in the menu to delete */
	for (s = sclist; s != NULL; s = s->next) {
		if (((s->menu & menu)) && !strcmp(s->keystr,keycopy)) {
			s->menu &= ~menu;
			DEBUG_LOG("deleted menu entry %d\n", s->menu);
		}
	}
}


/* Read and parse additional syntax files. */
void parse_include(char *ptr)
{
	struct stat rcinfo;
	char *option, *pinotrc_save = pinotrc, *expanded;
	size_t lineno_save = lineno;

	option = ptr;
	if (*option == '"') {
		option++;
	}
	ptr = parse_argument(ptr);

	/* Can't get the specified file's full path cause it may screw up
	our cwd depending on the parent dirs' permissions, (see Savannah bug 25297) */

	/* Don't open directories, character files, or block files. */
	if (stat(option, &rcinfo) != -1) {
		if (S_ISDIR(rcinfo.st_mode) || S_ISCHR(rcinfo.st_mode) || S_ISBLK(rcinfo.st_mode)) {
			rcfile_error(S_ISDIR(rcinfo.st_mode) ?
			             _("\"%s\" is a directory") :
			             _("\"%s\" is a device file"), option);
		}
	}

	expanded = real_dir_from_tilde(option);

	/* Open the new syntax file. */
	std::ifstream rcstream(expanded);
	if (!rcstream.is_open()) {
		rcfile_error(_("Error reading %s: %s"), expanded, strerror(errno));
		return;
	}

	/* Use the name and line number position of the new syntax file
	 * while parsing it, so we can know where any errors in it are. */
	pinotrc = expanded;
	lineno = 0;

	DEBUG_LOG("Parsing file \"%s\" (expanded from \"%s\")\n", expanded, option);

	parse_rcfile(rcstream
#ifdef ENABLE_COLOR
	             , true
#endif
	            );

	/* We're done with the new syntax file.  Restore the original
	 * filename and line number position. */
	pinotrc = pinotrc_save;
	lineno = lineno_save;

}

/* Return the numeric value corresponding to the color named in colorname,
 * and set bright to true if that color is bright (similarly for underline. */
COLORWIDTH color_name_to_value(const char *colorname, bool *bright, bool *underline)
{
	COLORWIDTH mcolor = -1;

	assert(colorname != NULL && bright != NULL && underline != NULL);

	if (strncasecmp(colorname, "bright", 6) == 0) {
		*bright = true;
		colorname += 6;
	} else if (strncasecmp(colorname, "bold", 4) == 0) {
		*bright = true;
		colorname += 4;
	}

	if (sscanf(colorname, "%hu", &mcolor) == 1) {
		if (mcolor > 255) {
			mcolor = 255;
		}
		return mcolor;
	}

	if (strncasecmp(colorname, "under", 5) == 0) {
		*underline = true;
		colorname += 5;
	}

	if (strcasecmp(colorname, "green") == 0) {
		mcolor = COLOR_GREEN;
	} else if (strcasecmp(colorname, "red") == 0) {
		mcolor = COLOR_RED;
	} else if (strcasecmp(colorname, "blue") == 0) {
		mcolor = COLOR_BLUE;
	} else if (strcasecmp(colorname, "white") == 0) {
		mcolor = COLOR_WHITE;
	} else if (strcasecmp(colorname, "yellow") == 0) {
		mcolor = COLOR_YELLOW;
	} else if (strcasecmp(colorname, "cyan") == 0) {
		mcolor = COLOR_CYAN;
	} else if (strcasecmp(colorname, "magenta") == 0) {
		mcolor = COLOR_MAGENTA;
	} else if (strcasecmp(colorname, "black") == 0) {
		mcolor = COLOR_BLACK;
	} else
		rcfile_error(N_("Color \"%s\" not understood.\n"
		                "Valid colors are \"green\", \"red\", \"blue\",\n"
		                "\"white\", \"yellow\", \"cyan\", \"magenta\" and\n"
		                "\"black\", with the optional prefix \"bright\"\n"
		                "for foreground colors."), colorname);

	return mcolor;
}

/* Parse the color string in the line at ptr, and add it to the current
 * file's associated colors.  If icase is true, treat the color string
 * as case insensitive. */
void parse_colors(char *ptr, bool icase)
{
	COLORWIDTH fg, bg;
	bool bright = false, underline = false, no_fgcolor = false;
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

	if (strchr(fgstr, ',') != NULL) {
		char *bgcolorname;

		strtok(fgstr, ",");
		bgcolorname = strtok(NULL, ",");
		if (bgcolorname == NULL) {
			/* If we have a background color without a foreground color,
			 * parse it properly. */
			bgcolorname = fgstr + 1;
			no_fgcolor = true;
		}
		if (strncasecmp(bgcolorname, "bright", 6) == 0 || strncasecmp(bgcolorname, "bold", 4) == 0) {
			rcfile_error(N_("Background color \"%s\" cannot be bright"), bgcolorname);
			return;
		}
		if (strncasecmp(bgcolorname, "under", 5) == 0) {
			rcfile_error(N_("Background color \"%s\" cannot be underline"), bgcolorname);
			return;
		}
		bg = color_name_to_value(bgcolorname, &bright, &underline);
	} else {
		bg = -1;
	}

	if (!no_fgcolor) {
		fg = color_name_to_value(fgstr, &bright, &underline);

		/* Don't try to parse screwed-up foreground colors. */
		if (fg == -1) {
			return;
		}
	} else {
		fg = -1;
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

		/* Save the starting regex string if it's valid, and set up the
		 * color information. */
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
				fprintf(stderr, "Starting a new colorstring for fg %hd, bg %hd\n", fg, bg);
			} else {
				fprintf(stderr, "Adding new entry for fg %hd, bg %hd\n", fg, bg);
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

			/* If the start regex was invalid, skip past the end regex to
			 * stay in sync. */
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

			DEBUG_LOG("Starting a new header entry: %s\n", regstr);

			new_syntax->headers.push_back(newheader);
		}
	}
}
#endif /* ENABLE_COLOR */

/* Check whether the user has unmapped every shortcut for a
sequence we consider 'vital', like the exit function */
static void check_vitals_mapped(void)
{
	subnfunc *f;
	int v;
#define VITALS 5
	void (*vitals[VITALS])(void) = { do_exit, do_exit, do_cancel, do_cancel, do_cancel };
	int inmenus[VITALS] = { MMAIN, MHELP, MWHEREIS, MREPLACE, MGOTOLINE };

	for  (v = 0; v < VITALS; v++) {
		for (f = allfuncs; f != NULL; f = f->next) {
			if (f->scfunc == vitals[v] && f->menus & inmenus[v]) {
				const sc *s = first_sc_for(inmenus[v], f->scfunc);
				if (!s) {
					rcfile_error(N_("Fatal error: no keys mapped for function \"%s\""), f->desc);
					fprintf(stderr, N_("Exiting.  Please use pinot with the -I option if needed to adjust your pinotrc settings\n"));
					exit(1);
				}
				break;
			}
		}
	}
}

/* Parse the rcfile, once it has been opened successfully at rcstream,
 * and close it afterwards.  If syntax_only is true, only allow the file
 * to contain color syntax commands: syntax, color, and icolor. */
void parse_rcfile(std::ifstream &rcstream
#ifdef ENABLE_COLOR
                  , bool syntax_only
#endif
                 )
{
	char *buf = NULL;
	ssize_t len;
	size_t n = 0;

	while ((len = getline(&buf, &n, rcstream)) > 0) {
		char *ptr, *keyword, *option;
		int set = 0;
		size_t i;

		/* Ignore the newline. */
		if (buf[len - 1] == '\n') {
			buf[len - 1] = '\0';
		}

		lineno++;
		ptr = buf;
		while (isblank(*ptr)) {
			ptr++;
		}

		/* If we have a blank line or a comment, skip to the next
		 * line. */
		if (*ptr == '\0' || *ptr == '#') {
			continue;
		}

		/* Otherwise, skip to the next space. */
		keyword = ptr;
		ptr = parse_next_word(ptr);

		/* Try to parse the keyword. */
		if (strcasecmp(keyword, "set") == 0) {
#ifdef ENABLE_COLOR
			if (syntax_only) {
				rcfile_error(N_("Command \"%s\" not allowed in included file"), keyword);
			} else
#endif
				set = 1;
		} else if (strcasecmp(keyword, "unset") == 0) {
#ifdef ENABLE_COLOR
			if (syntax_only) {
				rcfile_error(N_("Command \"%s\" not allowed in included file"), keyword);
			} else
#endif
				set = -1;
		}
#ifdef ENABLE_COLOR
		else if (strcasecmp(keyword, "include") == 0) {
			if (syntax_only) {
				rcfile_error(N_("Command \"%s\" not allowed in included file"), keyword);
			} else {
				parse_include(ptr);
			}
		} else if (strcasecmp(keyword, "syntax") == 0) {
			if (new_syntax != NULL && new_syntax->own_colors().empty()) {
				rcfile_error(N_("Syntax \"%s\" has no color commands"), new_syntax->desc.c_str());
			}
			parse_syntax(ptr);
		} else if (strcasecmp(keyword, "extends") == 0) {
			parse_extends(ptr);
		} else if (strcasecmp(keyword, "magic") == 0) {
			parse_magictype(ptr);
		} else if (strcasecmp(keyword, "header") == 0) {
			parse_headers(ptr);
		} else if (strcasecmp(keyword, "color") == 0) {
			parse_colors(ptr, false);
		} else if (strcasecmp(keyword, "icolor") == 0) {
			parse_colors(ptr, true);
		} else if (strcasecmp(keyword, "bind") == 0) {
			parse_keybinding(ptr);
		} else if (strcasecmp(keyword, "unbind") == 0) {
			parse_unbinding(ptr);
		}
#endif /* ENABLE_COLOR */
		else {
			rcfile_error(N_("Command \"%s\" not understood"), keyword);
		}

		if (set == 0) {
			continue;
		}

		if (*ptr == '\0') {
			rcfile_error(N_("Missing flag"));
			continue;
		}

		option = ptr;
		ptr = parse_next_word(ptr);

		bool found = false;
		for (auto iter = rcopts.begin(); iter != rcopts.end(); ++iter) {
			auto rcopt = *iter;
			if (rcopt.name == option) {
				found = true;
				DEBUG_LOG("parse_rcfile(): name = \"%s\"\n", rcopt.name.c_str());
				if (set == 1) {
					if (rcopt.flag != 0) {
						/* This option has a flag, so it doesn't take an argument. */
						SET(rcopt.flag);
					} else {
						/* This option doesn't have a flag, so it takes an argument. */
						if (*ptr == '\0') {
							rcfile_error(N_("Option \"%s\" requires an argument"), rcopt.name.c_str());
							break;
						}
						option = ptr;
						if (*option == '"') {
							option++;
						}
						ptr = parse_argument(ptr);

						option = mallocstrcpy(NULL, option);
						DEBUG_LOG("option = \"%s\"\n", option);

						/* Make sure option is a valid multibyte string. */
						if (!is_valid_mbstring(option)) {
							rcfile_error(N_("Option is not a valid multibyte string"));
							break;
						}

#ifndef DISABLE_OPERATINGDIR
						if (rcopt.name == "operatingdir") {
							operating_dir = option;
						} else
#endif
#ifndef DISABLE_WRAPJUSTIFY
							if (rcopt.name == "fill") {
								if (!parse_num(option, &wrap_at)) {
									rcfile_error(N_("Requested fill size \"%s\" is invalid"), option);
									wrap_at = -CHARS_FROM_EOL;
								} else {
									free(option);
								}
							} else
#endif
								if (rcopt.name == "matchbrackets") {
									matchbrackets = option;
									if (has_blank_mbchars(matchbrackets)) {
										rcfile_error(N_("Non-blank characters required"));
										free(matchbrackets);
										matchbrackets = NULL;
									}
								} else if (rcopt.name == "whitespace") {
									whitespace = option;
									if (mbstrlen(whitespace) != 2 || strlenpt(whitespace) != 2) {
										rcfile_error(N_("Two single-column characters required"));
										free(whitespace);
										whitespace = NULL;
									} else {
										whitespace_len[0] = parse_mbchar(whitespace, NULL, NULL);
										whitespace_len[1] = parse_mbchar(whitespace + whitespace_len[0], NULL, NULL);
									}
								} else
#ifdef ENABLE_JUSTIFY
									if (rcopt.name == "punct") {
										punct = option;
										if (has_blank_mbchars(punct)) {
											rcfile_error(N_("Non-blank characters required"));
											free(punct);
											punct = NULL;
										}
									} else if (rcopt.name == "brackets") {
										brackets = option;
										if (has_blank_mbchars(brackets)) {
											rcfile_error(N_("Non-blank characters required"));
											free(brackets);
											brackets = NULL;
										}
									} else if (rcopt.name == "quotestr") {
										quotestr = option;
									} else
#endif
										if (rcopt.name == "backupdir") {
											backup_dir = option;
										} else
#ifdef ENABLE_SPELLER
											if (rcopt.name == "speller") {
												alt_speller = option;
											} else
#endif
												if (rcopt.name == "tabsize") {
													if (!parse_num(option, &tabsize) || tabsize <= 0) {
														rcfile_error(N_("Requested tab size \"%s\" is invalid"), option);
														tabsize = -1;
													} else {
														free(option);
													}
												} else {
													assert(false);
												}
					}
					DEBUG_LOG("flag = %ld\n", rcopt.flag);
				} else if (rcopt.flag != 0) {
					UNSET(rcopt.flag);
				} else {
					rcfile_error(N_("Cannot unset flag \"%s\""), rcopt.name.c_str());
				}
				/* Looks like we still need this specific hack for undo */
				if (rcopt.name == "undo") {
					shortcut_init(0);
				}
				break;
			}
		}
		if (!found) {
			rcfile_error(N_("Unknown flag \"%s\""), option);
		}
	}

#ifdef ENABLE_COLOR
	if (new_syntax != NULL && new_syntax->own_colors().empty()) {
		rcfile_error(N_("Syntax \"%s\" has no color commands"), new_syntax->desc.c_str());
	}
#endif

	free(buf);
	lineno = 0;

	check_vitals_mapped();
	return;
}

/* The main rcfile function.  It tries to open the system-wide rcfile,
 * followed by the current user's rcfile. */
void do_rcfile(void)
{
	struct stat rcinfo;

	pinotrc = mallocstrcpy(pinotrc, SYSCONFDIR "/pinotrc");

	/* Don't open directories, character files, or block files. */
	if (stat(pinotrc, &rcinfo) != -1) {
		if (S_ISDIR(rcinfo.st_mode) || S_ISCHR(rcinfo.st_mode) || S_ISBLK(rcinfo.st_mode))
			rcfile_error(S_ISDIR(rcinfo.st_mode) ?
			             _("\"%s\" is a directory") :
			             _("\"%s\" is a device file"), pinotrc);
	}

	DEBUG_LOG("Parsing file \"%s\"\n", pinotrc);

	/* Try to open the system-wide pinotrc. */
	std::ifstream rcstream(pinotrc);
	if (rcstream.is_open()) {
		parse_rcfile(rcstream
#ifdef ENABLE_COLOR
		             , false
#endif
		            );
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

	if (homedir == NULL) {
		rcfile_error(N_("I can't find my home directory!  Wah!"));
	} else {
#ifndef RCFILE_NAME
#define RCFILE_NAME ".pinotrc"
#endif
		pinotrc = charealloc(pinotrc, strlen(homedir) + strlen(RCFILE_NAME) + 2);
		sprintf(pinotrc, "%s/%s", homedir, RCFILE_NAME);

		/* Don't open directories, character files, or block files. */
		if (stat(pinotrc, &rcinfo) != -1) {
			if (S_ISDIR(rcinfo.st_mode) || S_ISCHR(rcinfo.st_mode) || S_ISBLK(rcinfo.st_mode))
				rcfile_error(S_ISDIR(rcinfo.st_mode) ?
				             _("\"%s\" is a directory") :
				             _("\"%s\" is a device file"), pinotrc);
		}

		/* Try to open the current user's pinotrc. */
		rcstream.open(pinotrc);
		if (!rcstream.is_open()) {
			/* Don't complain about the file's not existing. */
			if (errno != ENOENT) {
				rcfile_error(N_("Error reading %s: %s"), pinotrc, strerror(errno));
			}
		} else
			parse_rcfile(rcstream
#ifdef ENABLE_COLOR
			             , false
#endif
			            );
	}

	free(pinotrc);
	pinotrc = NULL;

	if (errors && !ISSET(QUIET)) {
		errors = false;
		fprintf(stderr, _("\nPress Enter to continue starting pinot.\n"));
		while (getchar() != '\n') {
			;
		}
	}

#ifdef ENABLE_COLOR
	set_colorpairs();
#endif
}

#endif /* ENABLE_PINOTRC */
