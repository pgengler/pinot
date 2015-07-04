/**************************************************************************
 *   utils.c                                                              *
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

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <ctype.h>
#include <errno.h>

/* Return the user's home directory.  We use $HOME, and if that fails,
 * we fall back on the home directory of the effective user ID. */
void get_homedir(void)
{
	if (homedir == "") {
		const char *homenv = getenv("HOME");

		if (homenv == NULL) {
			const struct passwd *userage = getpwuid(geteuid());

			if (userage != NULL) {
				homenv = userage->pw_dir;
			}
		}
		homedir = std::string(homenv);
	}
}

/* Read a ssize_t from str, and store it in *val (if val is not NULL).
 * On error, we return false and don't change *val.  Otherwise, we
 * return true. */
bool parse_num(const char *str, ssize_t *val)
{
	char *first_error;
	ssize_t j;

	assert(str != NULL);

	/* Man page for strtol() says this is required, and
	   it looks like it is! */
	errno = 0;

	j = (ssize_t)strtol(str, &first_error, 10);

	if (errno == ERANGE || *str == '\0' || *first_error != '\0') {
		return false;
	}

	if (val != NULL) {
		*val = j;
	}

	return true;
}

/* Read two ssize_t's, separated by a comma, from str, and store them in
 * *line and *column (if they're not both NULL).  Return false on error,
 * or true otherwise. */
bool parse_line_column(const std::string& str, ssize_t *line, ssize_t *column)
{
	return parse_line_column(str.c_str(), line, column);
}

bool parse_line_column(const char *str, ssize_t *line, ssize_t *column)
{
	bool retval = true;
	const char *comma;

	assert(str != NULL);

	comma = strchr(str, ',');

	if (comma != NULL && column != NULL) {
		if (!parse_num(comma + 1, column)) {
			retval = false;
		}
	}

	if (line != NULL) {
		if (comma != NULL) {
			char *str_line = mallocstrncpy(NULL, str, comma - str + 1);
			str_line[comma - str] = '\0';

			if (str_line[0] != '\0' && !parse_num(str_line, line)) {
				retval = false;
			}

			free(str_line);
		} else if (!parse_num(str, line)) {
			retval = false;
		}
	}

	return retval;
}

/* Fix the memory allocation for a string. */
void align(char **str)
{
	assert(str != NULL);

	if (*str != NULL) {
		*str = charealloc(*str, strlen(*str) + 1);
	}
}

/* Null a string at a certain index and align it. */
void null_at(char **data, size_t index)
{
	assert(data != NULL);

	*data = charealloc(*data, index + 1);
	(*data)[index] = '\0';
}

/* For non-null-terminated lines.  A line, by definition, shouldn't
 * normally have newlines in it, so encode its nulls as newlines. */
void unsunder(std::string& str)
{
	std::replace(str.begin(), str.end(), '\0', '\n');
}

void unsunder(char *str, size_t true_len)
{
	assert(str != NULL);

	for (; true_len > 0; true_len--, str++) {
		if (*str == '\0') {
			*str = '\n';
		}
	}
}

/* For non-null-terminated lines.  A line, by definition, shouldn't
 * normally have newlines in it, so decode its newlines as nulls. */
void sunder(std::string& str)
{
	std::replace(str.begin(), str.end(), '\n', '\0');
}

void sunder(char *str)
{
	assert(str != NULL);

	for (; *str != '\0'; str++) {
		if (*str == '\n') {
			*str = '\0';
		}
	}
}

/* These functions, ngetline() (originally getline()) and ngetdelim()
 * (originally getdelim()), were adapted from GNU mailutils 0.5
 * (mailbox/getline.c).  Here is the notice from that file, after
 * converting to the GPL via LGPL clause 3, and with the Free Software
 * Foundation's address and the copyright years updated:
 *
 * GNU Mailutils -- a suite of utilities for electronic mail
 * Copyright (C) 1999, 2000, 2001, 2002, 2004, 2005, 2006, 2007
 * Free Software Foundation, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA. */

#ifndef HAVE_GETDELIM
/* This function is equivalent to getdelim(). */
ssize_t ngetdelim(char **lineptr, size_t *n, int delim, FILE *stream)
{
	size_t indx = 0;
	int c;

	/* Sanity checks. */
	if (lineptr == NULL || n == NULL || stream == NULL || fileno(stream) == -1) {
		errno = EINVAL;
		return -1;
	}

	/* Allocate the line the first time. */
	if (*lineptr == NULL) {
		*n = MAX_BUF_SIZE;
		*lineptr = charalloc(*n);
	}

	while ((c = getc(stream)) != EOF) {
		/* Check if more memory is needed. */
		if (indx >= *n) {
			*n += MAX_BUF_SIZE;
			*lineptr = charealloc(*lineptr, *n);
		}

		/* Put the result in the line. */
		(*lineptr)[indx++] = (char)c;

		/* Bail out. */
		if (c == delim) {
			break;
		}
	}

	/* Make room for the null character. */
	if (indx >= *n) {
		*n += MAX_BUF_SIZE;
		*lineptr = charealloc(*lineptr, *n);
	}

	/* Null-terminate the buffer. */
	null_at(lineptr, indx++);
	*n = indx;

	/* The last line may not have the delimiter.  We have to return what
	 * we got, and the error will be seen on the next iteration. */
	return (c == EOF && (indx - 1) == 0) ? -1 : indx - 1;
}
#endif

#ifndef HAVE_GETLINE
/* This function is equivalent to getline(). */
ssize_t ngetline(char **lineptr, size_t *n, FILE *stream)
{
	return getdelim(lineptr, n, '\n', stream);
}
#endif

ssize_t getdelim(char **lineptr, size_t *n, char delim, std::istream &stream)
{
	size_t indx = 0;

	/* Sanity checks. */
	if (lineptr == NULL || n == NULL) {
		errno = EINVAL;
		return -1;
	}

	/* Allocate the line the first time. */
	if (*lineptr == NULL) {
		*n = MAX_BUF_SIZE;
		*lineptr = charalloc(*n);
	}

	while (stream.good()) {
		char c = (char)stream.get();
		if (!stream.good()) {
			break;
		}
		/* Check if more memory is needed. */
		if (indx >= *n) {
			*n += MAX_BUF_SIZE;
			*lineptr = charealloc(*lineptr, *n);
		}

		/* Put the result in the line. */
		(*lineptr)[indx++] = c;

		/* Bail out. */
		if (c == delim) {
			break;
		}
	}

	/* Make room for the null character. */
	if (indx >= *n) {
		*n += MAX_BUF_SIZE;
		*lineptr = charealloc(*lineptr, *n);
	}

	/* Null-terminate the buffer. */
	null_at(lineptr, indx++);
	*n = indx;

	/* The last line may not have the delimiter.  We have to return what
	 * we got, and the error will be seen on the next iteration. */
	return (!stream.good() && (indx - 1) == 0) ? -1 : indx - 1;
}

ssize_t getline(char **lineptr, size_t *n, std::istream &stream)
{
	return getdelim(lineptr, n, '\n', stream);
}

/* Is the word starting at position pos in buf a whole word? */
bool is_whole_word(size_t pos, const char *buf, const char *word)
{
	char *p = charalloc(mb_cur_max()), *r = charalloc(mb_cur_max());
	size_t word_end = pos + strlen(word);
	bool retval;

	assert(buf != NULL && pos <= strlen(buf) && word != NULL);

	parse_mbchar(buf + move_mbleft(buf, pos), p, NULL);
	parse_mbchar(buf + word_end, r, NULL);

	/* If we're at the beginning of the line or the character before the
	 * word isn't a non-punctuation "word" character, and if we're at
	 * the end of the line or the character after the word isn't a
	 * non-punctuation "word" character, we have a whole word. */
	retval = (pos == 0 || !is_word_mbchar(p, false)) && (word_end == strlen(buf) || !is_word_mbchar(r, false));

	free(p);
	free(r);

	return retval;
}

/* If we are searching backwards, we will find the last match that
 * starts no later than start.  Otherwise we find the first match
 * starting no earlier than start.  If we are doing a regexp search, we
 * fill in the global variable regmatches with at most 9 subexpression
 * matches.  Also, all .rm_so elements are relative to the start of the
 * whole match, so regmatches[0].rm_so == 0. */
const char *strstrwrapper(const char *haystack, const char *needle, const char *start)
{
	/* start can be 1 character before the start or after the end of the
	 * line.  In either case, we just say no match was found. */
	if ((start > haystack && *(start - 1) == '\0') || start < haystack) {
		return NULL;
	}

	assert(haystack != NULL && needle != NULL && start != NULL);

	if (ISSET(USE_REGEXP)) {
		if (ISSET(BACKWARDS_SEARCH)) {
			if (regexec(&search_regexp, haystack, 1, regmatches, 0) == 0 && haystack + regmatches[0].rm_so <= start) {
				const char *retval = haystack + regmatches[0].rm_so;

				/* Search forward until there are no more matches. */
				while (regexec(&search_regexp, retval + 1, 1, regmatches, REG_NOTBOL) == 0 && retval + regmatches[0].rm_so + 1 <= start) {
					retval += regmatches[0].rm_so + 1;
				}
				/* Finally, put the subexpression matches in global variable
				 * regmatches.  The REG_NOTBOL flag doesn't matter now. */
				regexec(&search_regexp, retval, 10, regmatches, 0);
				return retval;
			}
		} else if (regexec(&search_regexp, start, 10, regmatches, (start > haystack) ? REG_NOTBOL : 0) == 0) {
			const char *retval = start + regmatches[0].rm_so;

			regexec(&search_regexp, retval, 10, regmatches, 0);
			return retval;
		}
		return NULL;
	}
	if (ISSET(CASE_SENSITIVE)) {
		if (ISSET(BACKWARDS_SEARCH)) {
			return revstrstr(haystack, needle, start);
		} else {
			return strstr(start, needle);
		}
	} else if (ISSET(BACKWARDS_SEARCH)) {
		return mbrevstrcasestr(haystack, needle, start);
	}
	return mbstrcasestr(start, needle);
}

/* This is a wrapper for the perror() function.  The wrapper temporarily
 * leaves curses mode, calls perror() (which writes to stderr), and then
 * reenters curses mode, updating the screen in the process.  Note that
 * nperror() causes the window to flicker once. */
void nperror(const char *s)
{
	endwin();
	perror(s);
	doupdate();
}

/* This is a wrapper for the malloc() function that properly handles
 * things when we run out of memory.  Thanks, BG, many people have been
 * asking for this... */
void *nmalloc(size_t howmuch)
{
	void *r = malloc(howmuch);

	if (r == NULL && howmuch != 0) {
		die(_("pinot is out of memory!"));
	}

	return r;
}

/* This is a wrapper for the realloc() function that properly handles
 * things when we run out of memory. */
void *nrealloc(void *ptr, size_t howmuch)
{
	void *r = realloc(ptr, howmuch);

	if (r == NULL && howmuch != 0) {
		die(_("pinot is out of memory!"));
	}

	return r;
}

/* Copy the first n characters of one malloc()ed string to another
 * pointer.  Should be used as: "dest = mallocstrncpy(dest, src,
 * n);". */
char *mallocstrncpy(char *dest, const char *src, size_t n)
{
	if (src == NULL) {
		src = "";
	}

	if (src != dest) {
		free(dest);
	}

	dest = charalloc(n);
	strncpy(dest, src, n);

	return dest;
}

/* Copy one malloc()ed string to another pointer.  Should be used as:
 * "dest = mallocstrcpy(dest, src);". */
char *mallocstrcpy(char *dest, const char *src)
{
	return mallocstrncpy(dest, src, (src == NULL) ? 1 : strlen(src) + 1);
}

/* pinot scrolls horizontally within a line in chunks.  Return the column
 * number of the first character displayed in the edit window when the
 * cursor is at the given column.  Note that (0 <= column -
 * get_page_start(column) < COLS). */
size_t get_page_start(size_t column)
{
	if (column == 0 || column < COLS - 1) {
		return 0;
	} else if (COLS > 8) {
		return column - 7 - (column - 7) % (COLS - 8);
	} else {
		return column - (COLS - 2);
	}
}

/* Return the placewewant associated with current_x, i.e. the zero-based
 * column position of the cursor.  The value will be no smaller than
 * current_x. */
size_t xplustabs(void)
{
	if (openfile->current) {
		return strnlenpt(openfile->current->data, openfile->current_x);
	} else {
		return 0;
	}
}

/* Return the index in s of the character displayed at the given column,
 * i.e. the largest value such that strnlenpt(s, actual_x(s, column)) <=
 * column. */
size_t actual_x(const char *s, size_t column)
{
	size_t i = 0;
	/* The position in s, returned. */
	size_t len = 0;
	/* The screen display width to s[i]. */

	assert(s != NULL);

	while (*s != '\0') {
		int s_len = parse_mbchar(s, NULL, &len);

		if (len > column) {
			break;
		}

		i += s_len;
		s += s_len;
	}

	return i;
}

/* A strnlen() with tabs and multicolumn characters factored in, similar
 * to xplustabs().  How many columns wide are the first maxlen characters
 * of s? */
size_t strnlenpt(const char *s, size_t maxlen)
{
	size_t len = 0;
	/* The screen display width to s[i]. */

	if (maxlen == 0) {
		return 0;
	}

	assert(s != NULL);

	while (*s != '\0') {
		int s_len = parse_mbchar(s, NULL, &len);

		s += s_len;

		if (maxlen <= s_len) {
			break;
		}

		maxlen -= s_len;
	}

	return len;
}

/* A strlen() with tabs and multicolumn characters factored in, similar
 * to xplustabs().  How many columns wide is s? */
size_t strlenpt(const char *s)
{
	return strnlenpt(s, (size_t)-1);
}

/* Append a new magicline to filebot. */
void new_magicline(void)
{
	openfile->filebot->next = new filestruct;
	openfile->filebot->next->data = mallocstrcpy(NULL, "");
	openfile->filebot->next->prev = openfile->filebot;
	openfile->filebot->next->next = NULL;
	openfile->filebot->next->lineno = openfile->filebot->lineno + 1;
	openfile->filebot = openfile->filebot->next;
	openfile->totsize++;
}

/* Remove the magicline from filebot, if there is one and it isn't the
 * only line in the file.  Assume that edittop and current are not at
 * filebot. */
void remove_magicline(void)
{
	if (openfile->filebot->data[0] == '\0' && openfile->filebot != openfile->fileage) {
		assert(openfile->filebot != openfile->edittop && openfile->filebot != openfile->current);

		openfile->filebot = openfile->filebot->prev;
		free_filestruct(openfile->filebot->next);
		openfile->filebot->next = NULL;
		openfile->totsize--;
	}
}

/* Set top_x and bot_x to the top and bottom x-coordinates of the mark,
 * respectively, based on the locations of top and bot.  If
 * right_side_up isn't NULL, set it to true if the mark begins with
 * (mark_begin, mark_begin_x) and ends with (current, current_x), or
 * false otherwise. */
void mark_order(const filestruct **top, size_t *top_x, const filestruct **bot, size_t *bot_x, bool *right_side_up)
{
	assert(top != NULL && top_x != NULL && bot != NULL && bot_x != NULL);

	if ((openfile->current->lineno == openfile->mark_begin->lineno && openfile->current_x > openfile->mark_begin_x) || openfile->current->lineno > openfile->mark_begin->lineno) {
		*top = openfile->mark_begin;
		*top_x = openfile->mark_begin_x;
		*bot = openfile->current;
		*bot_x = openfile->current_x;
		if (right_side_up != NULL) {
			*right_side_up = true;
		}
	} else {
		*bot = openfile->mark_begin;
		*bot_x = openfile->mark_begin_x;
		*top = openfile->current;
		*top_x = openfile->current_x;
		if (right_side_up != NULL) {
			*right_side_up = false;
		}
	}
}

/* Calculate the number of characters between begin and end, and return it. */
size_t get_totsize(const filestruct *begin, const filestruct *end)
{
	size_t totsize = 0;
	const filestruct *f;

	/* Go through the lines from begin to end->prev, if we can. */
	for (f = begin; f != end && f != NULL; f = f->next) {
		/* Count the number of characters on this line. */
		totsize += mbstrlen(f->data);

		/* Count the newline if we have one. */
		if (f->next != NULL) {
			totsize++;
		}
	}

	/* Go through the line at end, if we can. */
	if (f != NULL) {
		/* Count the number of characters on this line. */
		totsize += mbstrlen(f->data);

		/* Count the newline if we have one. */
		if (f->next != NULL) {
			totsize++;
		}
	}

	return totsize;
}

/* Get back a pointer given a line number in the current open file */
filestruct *fsfromline(ssize_t lineno)
{
	filestruct *f = openfile->current;

	if (lineno <= openfile->current->lineno)
		for (; f->lineno != lineno && f != openfile->fileage; f = f->prev) {
			;
		}
	else
		for (; f->lineno != lineno && f->next != NULL; f = f->next) {
			;
		}

	if (f->lineno != lineno) {
		f = NULL;
	}
	return f;
}

#ifdef DEBUG
/* Dump the filestruct inptr to stderr. */
void dump_filestruct(const filestruct *inptr)
{
	if (inptr == openfile->fileage) {
		DEBUG_LOG("Dumping file buffer to stderr...");
	} else if (inptr == cutbuffer) {
		DEBUG_LOG("Dumping cutbuffer to stderr...");
	} else {
		DEBUG_LOG("Dumping a buffer to stderr...");
	}

	while (inptr != NULL) {
		DEBUG_LOG('(' << inptr->lineno << ") " << inptr->data);
		inptr = inptr->next;
	}
}

/* Dump the current buffer's filestruct to stderr in reverse. */
void dump_filestruct_reverse(void)
{
	const filestruct *fileptr = openfile->filebot;

	while (fileptr != NULL) {
		DEBUG_LOG('(' << fileptr->lineno << ") " << fileptr->data);
		fileptr = fileptr->prev;
	}
}
#endif /* DEBUG */
