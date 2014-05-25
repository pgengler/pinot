/**************************************************************************
 *   color.c                                                              *
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

#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_MAGIC_H
#include <magic.h>
#endif

/* For each syntax list entry, go through the list of colors and assign
 * the color pairs. */
void set_colorpairs(void)
{
	bool defok = false;

	start_color();

#ifdef HAVE_USE_DEFAULT_COLORS
	/* Use the default colors, if available. */
	defok = (use_default_colors() != ERR);
#endif

	for (size_t i = 0; i < NUMBER_OF_ELEMENTS; i++) {
		COLORWIDTH fg, bg;
		bool bright = false, underline = false;
		if (parse_color_names(specified_color_combo[i], &fg, &bg, &bright, &underline)) {
			if (fg == -1 && !defok) {
				fg = COLOR_WHITE;
			}
			if (bg == -1 && !defok) {
				bg = COLOR_BLACK;
			}
DEBUG_LOG("Converted \"" << specified_color_combo[i] << "\" to fg=" << fg << " and bg=" << bg);
			init_pair(i + 1, fg, bg);
			interface_color_pair[i] = COLOR_PAIR(i + 1);
		} else if (i != FUNCTION_TAG) {
			interface_color_pair[i] = reverse_attr;
		}
	}

	for (auto pair : syntaxes) {
		auto this_syntax = pair.second;
		int color_pair = NUMBER_OF_ELEMENTS + 1;

		for (auto this_color : this_syntax->colors()) {
			if (this_color->pairnum == 0) {
				this_color->pairnum = color_pair++;
			}
		}
	}
}

/* Initialize the color information. */
void color_init(void)
{
	assert(openfile != openfiles.end());

	if (has_colors()) {
#ifdef HAVE_USE_DEFAULT_COLORS
		/* Use the default colors, if available. */
		bool defok = (use_default_colors() != ERR);
#endif

		for (auto tmpcolor : openfile->colorstrings) {
			short foreground = tmpcolor->fg, background = tmpcolor->bg;
			if (foreground == -1) {
#ifdef HAVE_USE_DEFAULT_COLORS
				if (!defok)
#endif
					foreground = COLOR_WHITE;
			}

			if (background == -1) {
#ifdef HAVE_USE_DEFAULT_COLORS
				if (!defok)
#endif
					background = COLOR_BLACK;
			}

			init_pair(tmpcolor->pairnum, foreground, background);

			DEBUG_LOG("init_pair(): fg = " << tmpcolor->fg << ", bg = " << tmpcolor->bg);
		}
	}
}

/* Update the color information based on the current filename. */
void color_update(void)
{
	Syntax *defsyntax = NULL;
	ColorList default_colors;

	assert(openfile != openfiles.end());

	openfile->syntax = NULL;
	openfile->colorstrings.clear();

	/* If we specified a syntax override string, use it. */
	if (syntaxstr != NULL) {
		/* If the syntax override is "none", it's the same as not having
		 * a syntax at all, so get out. */
		if (strcmp(syntaxstr, "none") == 0) {
			return;
		}

		for (auto pair : syntaxes) {
			auto tmpsyntax = pair.second;
			if (tmpsyntax->desc == syntaxstr) {
				openfile->syntax = tmpsyntax;
				openfile->colorstrings = tmpsyntax->colors();
				break;
			}
		}
	}

#ifdef HAVE_LIBMAGIC
	struct stat fileinfo;
	const char *magicstring = NULL;

	if (stat(openfile->filename, &fileinfo) == 0) {
		magic_t m = magic_open(MAGIC_SYMLINK |
#ifdef DEBUG
		               MAGIC_DEBUG | MAGIC_CHECK |
#endif /* DEBUG */
		               MAGIC_ERROR);
		if (m == NULL || magic_load(m, NULL) < 0) {
			std::cerr << "magic_load() failed: " << strerror(errno) << std::endl;
		} else {
			magicstring = magic_file(m, openfile->filename.c_str());
			if (magicstring == NULL) {
				std::cerr << "magic_file(" << openfile->filename << ") failed: " << magic_error(m) << std::endl;
			}
			DEBUG_LOG("magic string returned: " << magicstring);
		}
	}
#endif /* HAVE_LIBMAGIC */

	/* If we didn't specify a syntax override string, or if we did and
	 * there was no syntax by that name, get the syntax based on the
	 * file extension, and then look in the header. */
	if (openfile->colorstrings.empty()) {
		for (auto pair : syntaxes) {
			auto tmpsyntax = pair.second;

			/* If this is the default syntax, it has no associated
			 * extensions, which we've checked for elsewhere.  Skip over
			 * it here, but keep track of its color regexes. */
			if (tmpsyntax->desc == "default") {
				defsyntax = tmpsyntax;
				default_colors = tmpsyntax->colors();
				continue;
			}

			for (auto e : tmpsyntax->extensions) {
				if (e->matches(openfile->filename)) {
					openfile->syntax = tmpsyntax;
					openfile->colorstrings = tmpsyntax->colors();
					break;
				}
			}
		}

		/* Check magic if we don't yet have an answer */
#ifdef HAVE_LIBMAGIC
		if (openfile->colorstrings.empty()) {

			DEBUG_LOG("No match using extension, trying libmagic...");

			for (auto pair : syntaxes) {
				auto tmpsyntax = pair.second;
				for (auto e : tmpsyntax->magics) {
					if (magicstring && e->matches(magicstring)) {
						openfile->syntax = tmpsyntax;
						openfile->colorstrings = tmpsyntax->colors();
						break;
					}
				}
			}
		}
#endif /* HAVE_LIBMAGIC */

		/* If we haven't matched anything yet, try the headers */
		if (openfile->colorstrings.empty()) {
			DEBUG_LOG("No match for file extensions, looking at headers...");
			for (auto pair : syntaxes) {
				auto tmpsyntax = pair.second;

				for (auto e : tmpsyntax->headers) {

					/* Set colorstrings if we matched the extension
					 * regex. */
					if (e->matches(openfile->fileage->data)) {
						openfile->syntax = tmpsyntax;
						openfile->colorstrings = tmpsyntax->colors();
						break;
					}
				}
			}
		}
	}


	/* If we didn't get a syntax based on the file extension, and we
	 * have a default syntax, use it. */
	if (openfile->colorstrings.empty() && !default_colors.empty()) {
		openfile->syntax = defsyntax;
		openfile->colorstrings = default_colors;
	}

	for (auto tmpcolor : openfile->colorstrings) {
		/* tmpcolor->start_regex and tmpcolor->end_regex have already
		 * been checked for validity elsewhere.  Compile their specified
		 * regexes if we haven't already. */
		if (tmpcolor->start == NULL) {
			tmpcolor->start = new regex_t;
			regcomp(tmpcolor->start, tmpcolor->start_regex, REG_EXTENDED | (tmpcolor->icase ? REG_ICASE : 0));
		}

		if (tmpcolor->end_regex != NULL && tmpcolor->end == NULL) {
			tmpcolor->end = new regex_t;
			regcomp(tmpcolor->end, tmpcolor->end_regex, REG_EXTENDED | (tmpcolor->icase ? REG_ICASE : 0));
		}
	}
}

/* Reset the multicolor info cache for records for any lines which need
   to be recalculated */
void reset_multis_after(filestruct *fileptr, int mindex)
{
	filestruct *oof;
	for (oof = fileptr->next; oof != NULL; oof = oof->next) {
		alloc_multidata_if_needed(oof);
		if (oof->multidata == NULL) {
			continue;
		}
		if (oof->multidata[mindex] != CNONE) {
			oof->multidata[mindex] = -1;
		} else {
			break;
		}
	}
	for (; oof != NULL; oof = oof->next) {
		alloc_multidata_if_needed(oof);
		if (oof->multidata == NULL) {
			continue;
		}
		if (oof->multidata[mindex] == CNONE) {
			oof->multidata[mindex] = -1;
		} else {
			break;
		}
	}
	edit_refresh_needed = true;
}

void reset_multis_before(filestruct *fileptr, int mindex)
{
	filestruct *oof;
	for (oof = fileptr->prev; oof != NULL; oof = oof->prev) {
		alloc_multidata_if_needed(oof);
		if (oof->multidata == NULL) {
			continue;
		}
		if (oof->multidata[mindex] != CNONE) {
			oof->multidata[mindex] = -1;
		} else {
			break;
		}
	}
	for (; oof != NULL; oof = oof->prev) {
		alloc_multidata_if_needed(oof);
		if (oof->multidata == NULL) {
			continue;
		}
		if (oof->multidata[mindex] == CNONE) {
			oof->multidata[mindex] = -1;
		} else {
			break;
		}
	}

	edit_refresh_needed = true;
}

/* Reset one multiline regex info */
void reset_multis_for_id(filestruct *fileptr, int num)
{
	reset_multis_before(fileptr, num);
	reset_multis_after(fileptr, num);
	fileptr->multidata[num] = -1;
}

/* Reset multi line strings around a filestruct ptr, trying to be smart about stopping
   force = reset everything regardless, useful when we don't know how much screen state
           has changed  */
void reset_multis(filestruct *fileptr, bool force)
{
	int nobegin, noend;
	regmatch_t startmatch, endmatch;

	if (!openfile->syntax) {
		return;
	}

	for (auto tmpcolor : openfile->colorstrings) {

		/* If it's not a multi-line regex, amscray */
		if (tmpcolor->end == NULL) {
			continue;
		}

		alloc_multidata_if_needed(fileptr);
		if (force == true) {
			reset_multis_for_id(fileptr, tmpcolor->id);
			continue;
		}

		/* Figure out where the first begin and end are to determine if
		   things changed drastically for the precalculated multi values */
		nobegin = regexec(tmpcolor->start, fileptr->data, 1, &startmatch, 0);
		noend = regexec(tmpcolor->end, fileptr->data, 1, &endmatch, 0);
		if (fileptr->multidata[tmpcolor->id] ==  CWHOLELINE) {
			if (nobegin && noend) {
				continue;
			}
		} else if (fileptr->multidata[tmpcolor->id] == CNONE) {
			if (nobegin && noend) {
				continue;
			}
		}  else if (fileptr->multidata[tmpcolor->id] & CBEGINBEFORE && !noend && (nobegin || endmatch.rm_eo > startmatch.rm_eo)) {
			reset_multis_after(fileptr, tmpcolor->id);
			continue;
		}

		/* If we got here assume the worst */
		reset_multis_for_id(fileptr, tmpcolor->id);
	}
}
