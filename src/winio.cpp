/**************************************************************************
 *   winio.c                                                              *
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

#include <algorithm>

#include "proto.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

static int statusblank = 0;
/* The number of keystrokes left after we call statusbar(),
 * before we actually blank the statusbar. */
static bool disable_cursorpos = false;
/* Should we temporarily disable constant cursor position display? */

Key get_kbinput(WINDOW *win)
{
	// This is a hack but so far is the only way I've found to get
	// things to display correctly.
	wnoutrefresh(win);
	doupdate();
	Key key = keyboard->get_key();

	if (win == edit) {
		check_statusblank();
	}

	return key;
}

std::string get_verbatim_kbinput(WINDOW *win)
{
	/* Turn off flow control characters if necessary so that we can type them in verbatim */
	if (ISSET(PRESERVE)) {
		disable_flow_control();
	}
	std::string result = get_kbinput(win).verbatim();

	/* Turn flow control characters back on if necessary */
	if (ISSET(PRESERVE)) {
		enable_flow_control();
	}

	return result;
}

/* Return the shortcut corresponding to the values of kbinput, if any.
 * The shortcut will be the first one in the list (control key, meta key
 * sequence, function key, other meta key sequence) for the corresponding
 * function.  For example, passing in a meta key sequence that corresponds
 * to a function with a control key, a function key, and a meta key sequence
 * will return the control key corresponding to that function. */
const sc *get_shortcut(Key kbinput)
{
	std::string key = kbinput.format();

	DEBUG_LOG("get_shortcut(): key is " << key);

	for (auto s : sclist) {
		// If this shortcut doesn't apply to the current menu, skip it
		if (!(currmenu & s->menu)) {
			continue;
		}
		if (key == s->keystr) {
			DEBUG_LOG("matched seq \"" << s->keystr << "\" (menus " << currmenu << " = " << s->menu << ")");
			return s;
		}
	}

	DEBUG_LOG("get_shortcut(): matched nothing");

	return NULL;
}

/* Move to (x, y) in win, and display a line of n spaces with the
 * current attributes. */
void blank_line(WINDOW *win, int y, int x, int n)
{
	wmove(win, y, x);

	for (; n > 0; n--) {
		waddch(win, ' ');
	}
}

/* Blank the first line of the top portion of the window. */
void blank_titlebar(void)
{
	blank_line(topwin, 0, 0, COLS);
}

/* If the MORE_SPACE flag isn't set, blank the second line of the top
 * portion of the window. */
void blank_topbar(void)
{
	if (!ISSET(MORE_SPACE)) {
		blank_line(topwin, 1, 0, COLS);
	}
}

/* Blank all the lines of the middle portion of the window, i.e. the
 * edit window. */
void blank_edit(void)
{
	int i;

	for (i = 0; i < editwinrows; i++) {
		blank_line(edit, i, 0, COLS);
	}
}

/* Blank the first line of the bottom portion of the window. */
void blank_statusbar(void)
{
	blank_line(bottomwin, 0, 0, COLS);
}

/* If the NO_HELP flag isn't set, blank the last two lines of the bottom
 * portion of the window. */
void blank_bottombars(void)
{
	if (!ISSET(NO_HELP)) {
		blank_line(bottomwin, 1, 0, COLS);
		blank_line(bottomwin, 2, 0, COLS);
	}
}

/* Check if the number of keystrokes needed to blank the statusbar has
 * been pressed.  If so, blank the statusbar, unless constant cursor
 * position display is on. */
void check_statusblank(void)
{
	if (statusblank > 0) {
		statusblank--;

		if (statusblank == 0 && !ISSET(CONST_UPDATE)) {
			blank_statusbar();
			wnoutrefresh(bottomwin);
			reset_cursor();
			wnoutrefresh(edit);
		}
	}
}

/* Convert buf into a string that can be displayed on screen.  The
 * caller wants to display buf starting with column start_col, and
 * extending for at most len columns.  start_col is zero-based.  len is
 * one-based, so len == 0 means you get "" returned.  The returned
 * string is dynamically allocated, and should be freed.  If dollars is
 * true, the caller might put "$" at the beginning or end of the line if
 * it's too long. */
std::string display_string(const std::string& buf, size_t start_col, size_t len, bool dollars)
{
	char *foo = display_string(buf.c_str(), start_col, len, dollars);
	std::string result(foo);
	free(foo);

	return result;
}

char *display_string(const char *buf, size_t start_col, size_t len, bool dollars)
{
	size_t start_index;
	/* Index in buf of the first character shown. */
	size_t column;
	/* Screen column that start_index corresponds to. */
	size_t alloc_len;
	/* The length of memory allocated for converted. */
	char *converted;
	/* The string we return. */
	size_t index;
	/* Current position in converted. */
	char *buf_mb;
	int buf_mb_len;

	/* If dollars is true, make room for the "$" at the end of the
	 * line. */
	if (dollars && len > 0 && strlenpt(buf) > start_col + len) {
		len--;
	}

	if (len == 0) {
		return mallocstrcpy(NULL, "");
	}

	buf_mb = charalloc(mb_cur_max());

	start_index = actual_x(buf, start_col);
	column = strnlenpt(buf, start_index);

	assert(column <= start_col);

	/* Make sure there's enough room for the initial character, whether
	 * it's a multibyte control character, a non-control multibyte
	 * character, a tab character, or a null terminator.  Rationale:
	 *
	 * multibyte control character followed by a null terminator:
	 *     1 byte ('^') + mb_cur_max() bytes + 1 byte ('\0')
	 * multibyte non-control character followed by a null terminator:
	 *     mb_cur_max() bytes + 1 byte ('\0')
	 * tab character followed by a null terminator:
	 *     mb_cur_max() bytes + (tabsize - 1) bytes + 1 byte ('\0')
	 *
	 * Since tabsize has a minimum value of 1, it can substitute for 1
	 * byte above. */
	alloc_len = (mb_cur_max() + tabsize + 1) * MAX_BUF_SIZE;
	converted = charalloc(alloc_len);

	index = 0;

	if (buf[start_index] != '\0' && buf[start_index] != '\t' && (column < start_col || (dollars && column > 0))) {
		/* We don't display all of buf[start_index] since it starts to
		 * the left of the screen. */
		buf_mb_len = parse_mbchar(buf + start_index, buf_mb, NULL);

		if (is_cntrl_mbchar(buf_mb)) {
			if (column < start_col) {
				char *ctrl_buf_mb = charalloc(mb_cur_max());
				int ctrl_buf_mb_len, i;

				ctrl_buf_mb = control_mbrep(buf_mb, ctrl_buf_mb, &ctrl_buf_mb_len);

				for (i = 0; i < ctrl_buf_mb_len; i++) {
					converted[index++] = ctrl_buf_mb[i];
				}

				start_col += mbwidth(ctrl_buf_mb);

				free(ctrl_buf_mb);

				start_index += buf_mb_len;
			}
		} else if (using_utf8() && mbwidth(buf_mb) == 2) {
			if (column >= start_col) {
				converted[index++] = ' ';
				start_col++;
			}

			converted[index++] = ' ';
			start_col++;

			start_index += buf_mb_len;
		}
	}

	while (buf[start_index] != '\0') {
		buf_mb_len = parse_mbchar(buf + start_index, buf_mb, NULL);

		/* Make sure there's enough room for the next character, whether
		 * it's a multibyte control character, a non-control multibyte
		 * character, a tab character, or a null terminator. */
		if (index + mb_cur_max() + tabsize + 1 >= alloc_len - 1) {
			alloc_len += (mb_cur_max() + tabsize + 1) * MAX_BUF_SIZE;
			converted = charealloc(converted, alloc_len);
		}

		/* If buf contains a tab character, interpret it. */
		if (*buf_mb == '\t') {
			if (ISSET(WHITESPACE_DISPLAY)) {
				int i;

				for (i = 0; i < whitespace_len[0]; i++) {
					converted[index++] = whitespace[i];
				}
			} else {
				converted[index++] = ' ';
			}
			start_col++;
			while (start_col % tabsize != 0) {
				converted[index++] = ' ';
				start_col++;
			}
		} else if (is_cntrl_mbchar(buf_mb)) {
			/* If buf contains a control character, interpret it. */
			char *ctrl_buf_mb = charalloc(mb_cur_max());
			int ctrl_buf_mb_len, i;

			converted[index++] = '^';
			start_col++;

			ctrl_buf_mb = control_mbrep(buf_mb, ctrl_buf_mb, &ctrl_buf_mb_len);

			for (i = 0; i < ctrl_buf_mb_len; i++) {
				converted[index++] = ctrl_buf_mb[i];
			}

			start_col += mbwidth(ctrl_buf_mb);

			free(ctrl_buf_mb);
			/* If buf contains a space character, interpret it. */
		} else if (*buf_mb == ' ') {
			if (ISSET(WHITESPACE_DISPLAY)) {
				int i;

				for (i = whitespace_len[0]; i < whitespace_len[0] + whitespace_len[1]; i++) {
					converted[index++] = whitespace[i];
				}
			} else {
				converted[index++] = ' ';
			}
			start_col++;
		} else {
			/* If buf contains a non-control character, interpret it.  If buf
			 * contains an invalid multibyte sequence, display it as such. */
			char *nctrl_buf_mb = charalloc(mb_cur_max());
			int nctrl_buf_mb_len, i;

			/* Make sure an invalid sequence-starter byte is properly
			 * terminated, so that it doesn't pick up lingering bytes
			 * of any previous content. */
			null_at(&buf_mb, buf_mb_len);

			nctrl_buf_mb = mbrep(buf_mb, nctrl_buf_mb, &nctrl_buf_mb_len);

			for (i = 0; i < nctrl_buf_mb_len; i++) {
				converted[index++] = nctrl_buf_mb[i];
			}

			start_col += mbwidth(nctrl_buf_mb);

			free(nctrl_buf_mb);
		}

		start_index += buf_mb_len;
	}

	free(buf_mb);

	assert(alloc_len >= index + 1);

	/* Null-terminate converted. */
	converted[index] = '\0';

	/* Make sure converted takes up no more than len columns. */
	index = actual_x(converted, len);
	null_at(&converted, index);

	return converted;
}

/* If path is NULL, we're in normal editing mode, so display the current
 * version of pinot, the current filename, and whether the current file
 * has been modified on the titlebar.  If path isn't NULL, we're in the
 * file browser, and path contains the directory to start the file
 * browser in, so display the current version of pinot and the contents
 * of path on the titlebar. */
void titlebar(const std::string& path)
{
	titlebar(path.c_str());
}

void titlebar(const char *path)
{
	int space = COLS;
	/* The space we have available for display. */
	size_t verlen = strlenpt(PACKAGE_STRING) + 1;
	/* The length of the version message in columns, plus one for
	 * padding. */
	const char *prefix;
	/* "DIR:", "File:", or "New Buffer".  Goes before filename. */
	size_t prefixlen;
	/* The length of the prefix in columns, plus one for padding. */
	const char *state;
	/* "Modified", "View", or "".  Shows the state of this
	 * buffer. */
	ssize_t statelen = 0;
	/* The length of the state in columns, or the length of
	 * "Modified" if the state is blank and we're not in the file
	 * browser. */
	char *exppath = NULL;
	/* The filename, expanded for display. */
	bool newfie = false;
	/* Do we say "New Buffer"? */
	bool dots = false;
	/* Do we put an ellipsis before the path? */

	set_color(topwin, interface_colors[TITLE_BAR]);

	blank_titlebar();

	/* space has to be at least 4: two spaces before the version message,
	 * at least one character of the version message, and one space
	 * after the version message. */
	if (space < 4) {
		space = 0;
	} else {
		/* Limit verlen to 1/3 the length of the screen in columns,
		 * minus three columns for spaces. */
		if (verlen > (COLS / 3) - 3) {
			verlen = (COLS / 3) - 3;
		}
	}

	if (space >= 4) {
		/* Add a space after the version message, and account for both
		 * it and the two spaces before it. */
		mvwaddnstr(topwin, 0, 2, PACKAGE_STRING, actual_x(PACKAGE_STRING, verlen));
		verlen += 3;

		/* Account for the full length of the version message. */
		space -= verlen;
	}

	/* Don't display the state if we're in the file browser. */
	if (path != NULL) {
		state = "";
	} else {
		state = openfile->modified ? _("Modified") : ISSET(VIEW_MODE) ? _("View") : "";
	}

	statelen = strlenpt((*state == '\0' && path == NULL) ? _("Modified") : state);

	/* If possible, add a space before state. */
	if (space > 0 && statelen < space) {
		statelen++;
	} else {
		goto the_end;
	}

	/* path should be a directory if we're in the file browser. */
	if (path != NULL) {
		prefix = _("DIR:");
	} else {
		if (openfile->filename[0] == '\0') {
			prefix = _("New Buffer");
			newfie = true;
		} else {
			prefix = _("File:");
		}
	}

	prefixlen = strnlenpt(prefix, space - statelen) + 1;

	/* If newfie is false, add a space after prefix. */
	if (!newfie && prefixlen + statelen < space) {
		prefixlen++;
	}

	/* If we're not in the file browser, set path to the current
	 * filename. */
	if (path == NULL) {
		path = openfile->filename.c_str();
	}

	/* Account for the full lengths of the prefix and the state. */
	if (space >= prefixlen + statelen) {
		space -= prefixlen + statelen;
	} else {
		space = 0;
	}
	/* space is now the room we have for the filename. */

	if (!newfie) {
		size_t lenpt = strlenpt(path), start_col;

		/* Don't set dots to true if we have fewer than eight columns
		 * (i.e. one column for padding, plus seven columns for a
		 * filename). */
		dots = (space >= 8 && lenpt >= space);

		if (dots) {
			start_col = lenpt - space + 3;
			space -= 3;
		} else {
			start_col = 0;
		}

		exppath = display_string(path, start_col, space, false);
	}

	/* If dots is true, we will display something like "File:
	 * ...ename". */
	if (dots) {
		mvwaddnstr(topwin, 0, verlen - 1, prefix, actual_x(prefix, prefixlen));
		if (space <= -3 || newfie) {
			goto the_end;
		}
		waddch(topwin, ' ');
		waddnstr(topwin, "...", space + 3);
		if (space <= 0) {
			goto the_end;
		}
		waddstr(topwin, exppath);
	} else {
		size_t exppathlen = newfie ? 0 : strlenpt(exppath);
		/* The length of the expanded filename. */

		/* There is room for the whole filename, so we center it. */
		mvwaddnstr(topwin, 0, verlen + ((space - exppathlen) / 3), prefix, actual_x(prefix, prefixlen));
		if (!newfie) {
			waddch(topwin, ' ');
			waddstr(topwin, exppath);
		}
	}

the_end:
	free(exppath);

	if (state[0] != '\0') {
		if (statelen >= COLS - 1) {
			mvwaddnstr(topwin, 0, 0, state, actual_x(state, COLS));
		} else {
			assert(COLS - statelen - 1 >= 0);

			mvwaddnstr(topwin, 0, COLS - statelen - 1, state, actual_x(state, statelen));
		}
	}

	clear_color(topwin, interface_colors[TITLE_BAR]);

	wnoutrefresh(topwin);
	reset_cursor();
	wnoutrefresh(edit);
}

/* Mark the current file as modified if it isn't already, and then
 * update the titlebar to display the file's new status. */
void set_modified(void)
{
	if (!openfile->modified) {
		openfile->modified = true;
		titlebar(NULL);
		if (ISSET(LOCKING)) {
			if (openfile->filename == "") {
				/* Don't bother with a lockfile if there isn't an actual file open */
				return;
			} else if (openfile->lock_filename == "") {
				/* Translators: Try to keep this at most 80 characters. */
				statusbar(_("Warning: Modifying a file which is not locked, check directory permission?"));
			} else {
				write_lockfile(openfile->lock_filename, get_full_path(openfile->filename), true);
			}
		}
	}
}

/* Display a message on the statusbar, and set disable_cursorpos to
 * true, so that the message won't be immediately overwritten if
 * constant cursor position display is on. */
void statusbar(const char *msg, ...)
{
	va_list ap;
	char *bar, *foo;
	size_t start_x;
	bool old_whitespace;

	va_start(ap, msg);

	/* Curses mode is turned off.  If we use wmove() now, it will muck
	 * up the terminal settings.  So we just use vfprintf(). */
	if (isendwin()) {
		vfprintf(stderr, msg, ap);
		va_end(ap);
		return;
	}

	blank_statusbar();

	old_whitespace = ISSET(WHITESPACE_DISPLAY);
	UNSET(WHITESPACE_DISPLAY);
	bar = charalloc(mb_cur_max() * (COLS - 3));
	vsnprintf(bar, mb_cur_max() * (COLS - 3), msg, ap);
	va_end(ap);
	foo = display_string(bar, 0, COLS - 4, false);
	free(bar);
	if (old_whitespace) {
		SET(WHITESPACE_DISPLAY);
	}
	start_x = (COLS - strlenpt(foo) - 4) / 2;

	wmove(bottomwin, 0, start_x);
	set_color(bottomwin, interface_colors[STATUS_BAR]);
	waddstr(bottomwin, "[ ");
	waddstr(bottomwin, foo);
	free(foo);
	waddstr(bottomwin, " ]");
	clear_color(bottomwin, interface_colors[STATUS_BAR]);
	wnoutrefresh(bottomwin);
	reset_cursor();
	wnoutrefresh(edit);
	/* Leave the cursor at its position in the edit window, not in
	 * the statusbar. */

	disable_cursorpos = true;

	/* If we're doing quick statusbar blanking, and constant cursor
	 * position display is off, blank the statusbar after only one
	 * keystroke.  Otherwise, blank it after twenty-six keystrokes, as
	 * Pico does. */
	statusblank = ISSET(QUICK_BLANK) && !ISSET(CONST_UPDATE) ? 1 : 26;
}

/* Display the shortcut list in s on the last two rows of the bottom
 * portion of the window. */
void bottombars(int menu)
{
	size_t i = 0, colwidth, slen;
	const sc *s;

	/* Set the global variable to the given menu. */
	currmenu = menu;

	if (ISSET(NO_HELP)) {
		return;
	}

	if (menu == MMAIN) {
		slen = MAIN_VISIBLE;

		assert(slen <= length_of_list(menu));
	} else {
		slen = length_of_list(menu);

		/* Don't show any more shortcuts than the main list does. */
		if (slen > MAIN_VISIBLE) {
			slen = MAIN_VISIBLE;
		}
	}

	/* There will be this many characters per column, except for the
	 * last two, which will be longer by (COLS % colwidth) columns so as
	 * to not waste space.  We need at least three columns to display
	 * anything properly. */
	colwidth = COLS / ((slen / 2) + (slen % 2));

	blank_bottombars();

	DEBUG_LOG("In bottombars, and slen == " << slen);

	DEBUG_LOG("Checking menu items....");
	for (auto f : allfuncs) {
		if ((f->menus & menu) == 0) {
			continue;
		}

		if (f->desc == "") {
			continue;
		}

		DEBUG_LOG("found one! f->menus = " << f->menus << ", desc = \"" << f->desc << '"');
		s = first_sc_for(menu, f->scfunc);
		if (s == NULL) {
			DEBUG_LOG("Whoops, guess not, no shortcut key found for func!");
			continue;
		}
		wmove(bottomwin, 1 + i % 2, (i / 2) * colwidth);
		DEBUG_LOG("Calling onekey with keystr \"" << s->keystr << "\" and desc \"" << f->desc << '"');
		onekey(s->keystr, _(f->desc.c_str()), colwidth + (COLS % colwidth));
		if (++i >= slen) {
			break;
		}
	}

	wnoutrefresh(bottomwin);
	reset_cursor();
	wnoutrefresh(edit);
}

/* Write a shortcut key to the help area at the bottom of the window.
 * keystroke is e.g. "^G" and desc is e.g. "Get Help".  We are careful
 * to write at most len characters, even if len is very small and
 * keystroke and desc are long.  Note that waddnstr(,,(size_t)-1) adds
 * the whole string!  We do not bother padding the entry with blanks. */
void onekey(const std::string& keystroke, const std::string& desc, size_t len)
{
	size_t keystroke_len = keystroke.length() + 1;

	set_color(bottomwin, interface_colors[KEY_COMBO]);
	waddnstr(bottomwin, keystroke.c_str(), actual_x(keystroke.c_str(), len));
	clear_color(bottomwin, interface_colors[KEY_COMBO]);

	if (len > keystroke_len) {
		len -= keystroke_len;
	} else {
		len = 0;
	}

	if (len > 0) {
		waddch(bottomwin, ' ');
		waddnstr(bottomwin, desc.c_str(), actual_x(desc.c_str(), len));
	}
}

/* Reset current_y, based on the position of current, and put the cursor
 * in the edit window at (current_y, current_x). */
void reset_cursor(void)
{
	size_t xpt;
	/* If we haven't opened any files yet, put the cursor in the top
	 * left corner of the edit window and get out. */
	if (openfiles.size() == 0) {
		wmove(edit, 0, 0);
		return;
	}

	xpt = xplustabs();

	if (ISSET(SOFTWRAP)) {
		filestruct *tmp;
		openfile->current_y = 0;

		for (tmp = openfile->edittop; tmp && tmp != openfile->current; tmp = tmp->next) {
			openfile->current_y += 1 + strlenpt(tmp->data) / COLS;
		}

		openfile->current_y += xplustabs() / COLS;
		if (openfile->current_y < editwinrows) {
			wmove(edit, openfile->current_y, xpt % COLS);
		}
	} else {
		openfile->current_y = openfile->current->lineno - openfile->edittop->lineno;

		if (openfile->current_y < editwinrows) {
			wmove(edit, openfile->current_y, xpt - get_page_start(xpt));
		}
	}
}

void unset_formatting(ColorPtr color)
{
	wattroff(edit, A_BOLD);
	wattroff(edit, A_UNDERLINE);
	wattroff(edit, COLOR_PAIR(color->pairnum));
}

/* edit_draw() takes care of the job of actually painting a line into
 * the edit window.  fileptr is the line to be painted, at row line of
 * the window.  converted is the actual string to be written to the
 * window, with tabs and control characters replaced by strings of
 * regular characters.  start is the column number of the first
 * character of this page.  That is, the first character of converted
 * corresponds to character number actual_x(fileptr->data, start) of the
 * line. */
void edit_draw(filestruct *fileptr, const char *converted, int line, size_t start)
{
	size_t startpos = actual_x(fileptr->data, start);
	/* The position in fileptr->data of the leftmost character
	 * that displays at least partially on the window. */
	size_t endpos = actual_x(fileptr->data, start + COLS - 1) + 1;
	/* The position in fileptr->data of the first character that is
	 * completely off the window to the right.
	 *
	 * Note that endpos might be beyond the null terminator of the
	 * string. */

	assert(openfile != openfiles.end() && fileptr != NULL && converted != NULL);
	assert(strlenpt(converted) <= COLS);

	/* Just paint the string in any case (we'll add color or reverse on
	 * just the text that needs it). */
	mvwaddstr(edit, line, 0, converted);
	/* Tell ncurses to really redraw the line without trying to optimize
	 * for what it thinks is already there, because it gets it wrong in
	 * the case of a wide character in column zero. */
#ifndef USE_SLANG
	wredrawln(edit, line, 1);
#endif

	/* If color syntaxes are available and turned on, we need to display
	 * them. */
	if (!openfile->colorstrings.empty() && !ISSET(NO_COLOR_SYNTAX)) {
		/* Set up multi-line color data for this line if it's not yet calculated  */
		if (fileptr->multidata == NULL && openfile->syntax && openfile->syntax->nmultis > 0) {
			int i;
			fileptr->multidata = (short *) nmalloc(openfile->syntax->nmultis * sizeof(short));
			for (i = 0; i < openfile->syntax->nmultis; i++) {
				fileptr->multidata[i] = -1;    /* Assue this applies until we know otherwise */
			}

		}
		for (auto tmpcolor : openfile->colorstrings) {
			int x_start;
			/* Starting column for mvwaddnstr.  Zero-based. */
			int paintlen;
			/* Number of chars to paint on this line.  There are
			 * COLS characters on a whole line. */
			size_t index;
			/* Index in converted where we paint. */
			regmatch_t startmatch;
			/* Match position for start_regex. */
			regmatch_t endmatch;
			/* Match position for end_regex. */

			if (tmpcolor->bright) {
				wattron(edit, A_BOLD);
			}
			if (tmpcolor->underline) {
				wattron(edit, A_UNDERLINE);
			}
			wattron(edit, COLOR_PAIR(tmpcolor->pairnum));
			/* Two notes about regexec().  A return value of zero means
			 * that there is a match.  Also, rm_eo is the first
			 * non-matching character after the match. */

			/* First case,tmpcolor is a single-line expression. */
			if (tmpcolor->end == NULL) {
				size_t k = 0;

				/* We increment k by rm_eo, to move past the end of the
				 * last match.  Even though two matches may overlap, we
				 * want to ignore them, so that we can highlight e.g. C
				 * strings correctly. */
				while (k < endpos) {
					/* Note the fifth parameter to regexec().  It says
					 * not to match the beginning-of-line character
					 * unless k is zero.  If regexec() returns
					 * REG_NOMATCH, there are no more matches in the
					 * line. */
					if (regexec(tmpcolor->start, &fileptr->data[k], 1, &startmatch, (k == 0) ? 0 : REG_NOTBOL) == REG_NOMATCH) {
						break;
					}


					/* Translate the match to the beginning of the
					 * line. */
					startmatch.rm_so += k;
					startmatch.rm_eo += k;

					/* Skip over a zero-length regex match. */
					if (startmatch.rm_so == startmatch.rm_eo) {
						startmatch.rm_eo++;
					} else if (startmatch.rm_so < endpos && startmatch.rm_eo > startpos) {
						x_start = (startmatch.rm_so <= startpos) ? 0 : strnlenpt(fileptr->data, startmatch.rm_so) - start;

						index = actual_x(converted, x_start);

						paintlen = actual_x(converted + index, strnlenpt(fileptr->data, startmatch.rm_eo) - start - x_start);

						assert(0 <= x_start && 0 <= paintlen);

						mvwaddnstr(edit, line, x_start, converted + index, paintlen);
					}
					k = startmatch.rm_eo;
				}
			} else if (fileptr->multidata != NULL && fileptr->multidata[tmpcolor->id] != CNONE) {
				/* This is a multi-line regex.  There are two steps.
				 * First, we have to see if the beginning of the line is
				 * colored by a start on an earlier line, and an end on
				 * this line or later.
				 *
				 * We find the first line before fileptr matching the
				 * start.  If every match on that line is followed by an
				 * end, then go to step two.  Otherwise, find the next
				 * line after start_line matching the end.  If that line
				 * is not before fileptr, then paint the beginning of
				 * this line. */
				const filestruct *start_line = fileptr->prev;
				/* The first line before fileptr matching start. */
				regoff_t start_col;
				/* Where it starts in that line. */
				const filestruct *end_line;
				short md = fileptr->multidata[tmpcolor->id];

				if (md == -1) {
					fileptr->multidata[tmpcolor->id] = CNONE;    /* until we find out otherwise */
				} else if (md == CNONE) {
					unset_formatting(tmpcolor);
					continue;
				} else if (md == CWHOLELINE) {
					mvwaddnstr(edit, line, 0, converted, -1);
					unset_formatting(tmpcolor);
					continue;
				} else if (md == CBEGINBEFORE) {
					regexec(tmpcolor->end, fileptr->data, 1, &endmatch, 0);
					paintlen = actual_x(converted, strnlenpt(fileptr->data, endmatch.rm_eo) - start);
					mvwaddnstr(edit, line, 0, converted, paintlen);
					unset_formatting(tmpcolor);
					continue;
				}

				while (start_line != NULL && regexec(tmpcolor->start, start_line->data, 1, &startmatch, 0) == REG_NOMATCH) {
					/* If there is an end on this line, there is no need
					 * to look for starts on earlier lines. */
					if (regexec(tmpcolor->end, start_line->data, 0, NULL, 0) == 0) {
						goto step_two;
					}
					start_line = start_line->prev;
				}

				/* Skip over a zero-length regex match. */
				if (startmatch.rm_so == startmatch.rm_eo) {
					startmatch.rm_eo++;
				} else {
					/* No start found, so skip to the next step. */
					if (start_line == NULL) {
						goto step_two;
					}
					/* Now start_line is the first line before fileptr
					 * containing a start match.  Is there a start on
					 * this line not followed by an end on this line? */
					start_col = 0;
					while (true) {
						start_col += startmatch.rm_so;
						startmatch.rm_eo -= startmatch.rm_so;
						if (regexec(tmpcolor->end, start_line->data + start_col + startmatch.rm_eo, 0, NULL, (start_col + startmatch.rm_eo == 0) ? 0 : REG_NOTBOL) == REG_NOMATCH) {
							/* No end found after this start. */
							break;
						}
						start_col++;
						if (regexec(tmpcolor->start, start_line->data + start_col, 1, &startmatch, REG_NOTBOL) == REG_NOMATCH) {
							/* No later start on this line. */
							goto step_two;
						}
					}
					/* Indeed, there is a start not followed on this
					 * line by an end. */

					/* We have already checked that there is no end
					 * before fileptr and after the start.  Is there an
					 * end after the start at all?  We don't paint
					 * unterminated starts. */
					end_line = fileptr;
					while (end_line != NULL && regexec(tmpcolor->end, end_line->data, 1, &endmatch, 0) == REG_NOMATCH) {
						end_line = end_line->next;
					}

					/* No end found, or it is too early. */
					if (end_line == NULL || (end_line == fileptr && endmatch.rm_eo <= startpos)) {
						goto step_two;
					}

					/* Now paint the start of fileptr.  If the start of
					 * fileptr is on a different line from the end,
					 * paintlen is -1, meaning that everything on the
					 * line gets painted.  Otherwise, paintlen is the
					 * expanded location of the end of the match minus
					 * the expanded location of the beginning of the
					 * page. */
					if (end_line != fileptr) {
						paintlen = -1;
						fileptr->multidata[tmpcolor->id] = CWHOLELINE;
					} else {
						paintlen = actual_x(converted, strnlenpt(fileptr->data, endmatch.rm_eo) - start);
						fileptr->multidata[tmpcolor->id] = CBEGINBEFORE;
					}
					mvwaddnstr(edit, line, 0, converted, paintlen);
step_two:
					/* Second step, we look for starts on this line. */
					start_col = 0;

					while (start_col < endpos) {
						if (regexec(tmpcolor->start, fileptr->data + start_col, 1, &startmatch, (start_col == 0) ? 0 : REG_NOTBOL) == REG_NOMATCH || start_col + startmatch.rm_so >= endpos) {
							/* No more starts on this line. */
							break;
						}
						/* Translate the match to be relative to the
						 * beginning of the line. */
						startmatch.rm_so += start_col;
						startmatch.rm_eo += start_col;

						x_start = (startmatch.rm_so <= startpos) ? 0 : strnlenpt(fileptr->data, startmatch.rm_so) - start;

						index = actual_x(converted, x_start);

						if (regexec(tmpcolor->end, fileptr->data + startmatch.rm_eo, 1, &endmatch, (startmatch.rm_eo == 0) ? 0 : REG_NOTBOL) == 0) {
							/* Translate the end match to be relative to the beginning of the line. */
							endmatch.rm_so += startmatch.rm_eo;
							endmatch.rm_eo += startmatch.rm_eo;
							/* There is an end on this line.  But does
							 * it appear on this page, and is the match
							 * more than zero characters long? */
							if (endmatch.rm_eo > startpos && endmatch.rm_eo > startmatch.rm_so) {
								paintlen = actual_x(converted + index, strnlenpt(fileptr->data, endmatch.rm_eo) - start - x_start);

								assert(0 <= x_start && x_start < COLS);

								mvwaddnstr(edit, line, x_start, converted + index, paintlen);
								if (paintlen > 0) {
									fileptr->multidata[tmpcolor->id] = CSTARTENDHERE;
								}

							}
						} else {
							/* There is no end on this line.  But we
							 * haven't yet looked for one on later
							 * lines. */
							end_line = fileptr->next;

							while (end_line != NULL && regexec(tmpcolor->end, end_line->data, 0, NULL, 0) == REG_NOMATCH) {
								end_line = end_line->next;
							}

							if (end_line != NULL) {
								assert(0 <= x_start && x_start < COLS);

								mvwaddnstr(edit, line, x_start, converted + index, -1);
								/* We painted to the end of the line, so
								 * don't bother checking any more
								 * starts. */
								fileptr->multidata[tmpcolor->id] = CENDAFTER;
								break;
							}
						}
						start_col = startmatch.rm_so + 1;
					}
				}
			}
			unset_formatting(tmpcolor);
		}
	}

	/* If the mark is on, we need to display it. */
	if (openfile->mark_set && (fileptr->lineno <=
	                           openfile->mark_begin->lineno || fileptr->lineno <=
	                           openfile->current->lineno) && (fileptr->lineno >=
	                                   openfile->mark_begin->lineno || fileptr->lineno >=
	                                   openfile->current->lineno)) {
		/* fileptr is at least partially selected. */
		const filestruct *top;
		/* Either current or mark_begin, whichever is first. */
		size_t top_x;
		/* current_x or mark_begin_x, corresponding to top. */
		const filestruct *bot;
		size_t bot_x;
		int x_start;
		/* Starting column for mvwaddnstr().  Zero-based. */
		int paintlen;
		/* Number of characters to paint on this line.  There are
		 * COLS characters on a whole line. */
		size_t index;
		/* Index in converted where we paint. */

		mark_order(&top, &top_x, &bot, &bot_x, NULL);

		if (top->lineno < fileptr->lineno || top_x < startpos) {
			top_x = startpos;
		}
		if (bot->lineno > fileptr->lineno || bot_x > endpos) {
			bot_x = endpos;
		}

		/* The selected bit of fileptr is on this page. */
		if (top_x < endpos && bot_x > startpos) {
			assert(startpos <= top_x);

			/* x_start is the expanded location of the beginning of the
			 * mark minus the beginning of the page. */
			x_start = strnlenpt(fileptr->data, top_x) - start;

			/* If the end of the mark is off the page, paintlen is -1,
			 * meaning that everything on the line gets painted.
			 * Otherwise, paintlen is the expanded location of the end
			 * of the mark minus the expanded location of the beginning
			 * of the mark. */
			if (bot_x >= endpos) {
				paintlen = -1;
			} else
				paintlen = strnlenpt(fileptr->data, bot_x) - (x_start + start);

			/* If x_start is before the beginning of the page, shift
			 * paintlen x_start characters to compensate, and put
			 * x_start at the beginning of the page. */
			if (x_start < 0) {
				paintlen += x_start;
				x_start = 0;
			}

			assert(x_start >= 0 && x_start <= strlen(converted));

			index = actual_x(converted, x_start);

			if (paintlen > 0) {
				paintlen = actual_x(converted + index, paintlen);
			}

			set_color(edit, interface_colors[FUNCTION_TAG]);
			mvwaddnstr(edit, line, x_start, converted + index, paintlen);
			clear_color(edit, interface_colors[FUNCTION_TAG]);
		}
	}
}

/* Just update one line in the edit buffer.  This is basically a wrapper
 * for edit_draw().  The line will be displayed starting with
 * fileptr->data[index].  Likely arguments are current_x or zero.
 * Returns: Number of additiona lines consumed (needed for SOFTWRAP)
 */
int update_line(filestruct *fileptr, size_t index)
{
	int line = 0;
	int extralinesused = 0;
	/* The line in the edit window that we want to update. */
	char *converted;
	/* fileptr->data converted to have tabs and control characters
	 * expanded. */
	size_t page_start;
	filestruct *tmp;

	assert(fileptr != NULL);

	if (ISSET(SOFTWRAP)) {
		for (tmp = openfile->edittop; tmp && tmp != fileptr; tmp = tmp->next) {
			line += 1 + (strlenpt(tmp->data) / COLS);
		}
	} else {
		line = fileptr->lineno - openfile->edittop->lineno;
	}

	if (line < 0 || line >= editwinrows) {
		return 1;
	}

	/* First, blank out the line. */
	blank_line(edit, line, 0, COLS);

	/* Next, convert variables that index the line to their equivalent
	 * positions in the expanded line. */
	if (ISSET(SOFTWRAP)) {
		index = 0;
	} else {
		index = strnlenpt(fileptr->data, index);
	}
	page_start = get_page_start(index);

	/* Expand the line, replacing tabs with spaces, and control
	 * characters with their displayed forms. */
	converted = display_string(fileptr->data, page_start, COLS, !ISSET(SOFTWRAP));

#ifdef DEBUG
	if (ISSET(SOFTWRAP) && strlen(converted) >= COLS - 2) {
		fprintf(stderr, "update_line(): converted(1) line = %s\n", converted);
	}
#endif


	/* Paint the line. */
	edit_draw(fileptr, converted, line, page_start);
	free(converted);

	if (!ISSET(SOFTWRAP)) {
		if (page_start > 0) {
			mvwaddch(edit, line, 0, '$');
		}
		if (strlenpt(fileptr->data) > page_start + COLS) {
			mvwaddch(edit, line, COLS - 1, '$');
		}
	} else {
		int full_length = strlenpt(fileptr->data);
		for (index += COLS; index <= full_length && line < editwinrows; index += COLS) {
			line++;
			DEBUG_LOG("update_line(): Softwrap code, moving to " << line << " index " << index);
			blank_line(edit, line, 0, COLS);

			/* Expand the line, replacing tabs with spaces, and control
			 * characters with their displayed forms. */
			converted = display_string(fileptr->data, index, COLS, !ISSET(SOFTWRAP));
			if (ISSET(SOFTWRAP) && strlen(converted) >= COLS - 2) {
				DEBUG_LOG("update_line(): converted(2) line == " << converted);
			}

			/* Paint the line. */
			edit_draw(fileptr, converted, line, index);
			free(converted);
			extralinesused++;
		}
	}
	return extralinesused;
}

/* Return true if we need an update after moving horizontally, and false
 * otherwise.  We need one if the mark is on or if pww_save and
 * placewewant are on different pages. */
bool need_horizontal_update(size_t pww_save)
{
	return openfile->mark_set || get_page_start(pww_save) != get_page_start(openfile->placewewant);
}

/* Return true if we need an update after moving vertically, and false
 * otherwise.  We need one if the mark is on or if pww_save and
 * placewewant are on different pages. */
bool need_vertical_update(size_t pww_save)
{
	return openfile->mark_set || get_page_start(pww_save) != get_page_start(openfile->placewewant);
}

/* When edittop changes, try and figure out how many lines
 * we really have to work with (i.e. set maxrows)
 */
void compute_maxrows(void)
{
	int n;
	filestruct *foo = openfile->edittop;

	if (!ISSET(SOFTWRAP)) {
		maxrows = editwinrows;
		return;
	}

	maxrows = 0;
	for (n = 0; n < editwinrows && foo; n++) {
		maxrows ++;
		n += strlenpt(foo->data) / COLS;
		foo = foo->next;
	}

	if (n < editwinrows) {
		maxrows += editwinrows - n;
	}

	DEBUG_LOG("compute_maxrows(): maxrows = " << maxrows);
}

/* Scroll the edit window in the given direction and the given number
 * of lines, and draw new lines on the blank lines left after the
 * scrolling.  direction is the direction to scroll, either UPWARD or
 * DOWNWARD, and nlines is the number of lines to scroll.  We change
 * edittop, and assume that current and current_x are up to date.  We
 * also assume that scrollok(edit) is false. */
void edit_scroll(ScrollDir direction, ssize_t nlines)
{
	filestruct *foo;
	ssize_t i;
	bool do_redraw = false;

	/* Don't bother scrolling less than one line. */
	if (nlines < 1) {
		return;
	}

	if (need_vertical_update(0)) {
		do_redraw = true;
	}

	/* Part 1: nlines is the number of lines we're going to scroll the
	 * text of the edit window. */

	/* Move the top line of the edit window up or down (depending on the
	 * value of direction) nlines lines, or as many lines as we can if
	 * there are fewer than nlines lines available. */
	for (i = nlines; i > 0; i--) {
		if (direction == UPWARD) {
			if (openfile->edittop == openfile->fileage) {
				break;
			}
			openfile->edittop = openfile->edittop->prev;
		} else {
			if (openfile->edittop == openfile->filebot) {
				break;
			}
			openfile->edittop = openfile->edittop->next;
		}
		/* Don't over-scroll on long lines */
		if (ISSET(SOFTWRAP) && (direction == UPWARD)) {
			ssize_t len = strlenpt(openfile->edittop->data) / COLS;
			i -= len;
			if (len > 0) {
				do_redraw = true;
			}
		}
	}

	/* Limit nlines to the number of lines we could scroll. */
	nlines -= i;

	/* Don't bother scrolling zero lines or more than the number of
	 * lines in the edit window minus one; in both cases, get out, and
	 * call edit_refresh() beforehand if we need to. */
	if (nlines == 0 || do_redraw || nlines >= editwinrows) {
		if (do_redraw || nlines >= editwinrows) {
			edit_refresh_needed = true;
		}
		return;
	}

	/* Scroll the text of the edit window up or down nlines lines,
	 * depending on the value of direction. */
	scrollok(edit, true);
	wscrl(edit, (direction == UPWARD) ? -nlines : nlines);
	scrollok(edit, false);

	/* Part 2: nlines is the number of lines in the scrolled region of
	 * the edit window that we need to draw. */

	/* If the top or bottom line of the file is now visible in the edit
	 * window, we need to draw the entire edit window. */
	if ((direction == UPWARD && openfile->edittop == openfile->fileage)
		|| (direction == DOWNWARD && openfile->edittop->lineno + editwinrows - 1 >= openfile->filebot->lineno)) {
		nlines = editwinrows;
	}

	/* If the scrolled region contains only one line, and the line
	 * before it is visible in the edit window, we need to draw it too.
	 * If the scrolled region contains more than one line, and the lines
	 * before and after the scrolled region are visible in the edit
	 * window, we need to draw them too. */
	nlines += (nlines == 1) ? 1 : 2;

	if (nlines > editwinrows) {
		nlines = editwinrows;
	}

	/* If we scrolled up, we're on the line before the scrolled region. */
	foo = openfile->edittop;

	/* If we scrolled down, move down to the line before the scrolled region. */
	if (direction == DOWNWARD) {
		for (i = editwinrows - nlines; i > 0 && foo != NULL; i--) {
			foo = foo->next;
		}
	}

	/* Draw new lines on any blank lines before or inside the scrolled
	 * region.  If we scrolled down and we're on the top line, or if we
	 * scrolled up and we're on the bottom line, the line won't be
	 * blank, so we don't need to draw it unless the mark is on or we're
	 * not on the first page. */
	for (i = nlines; i > 0 && foo != NULL; i--) {
		if ((i == nlines && direction == DOWNWARD) || (i == 1 && direction == UPWARD)) {
			if (do_redraw)
				update_line(foo, (foo == openfile->current) ? openfile->current_x : 0);
		} else {
			update_line(foo, (foo == openfile->current) ? openfile->current_x : 0);
		}
		foo = foo->next;
	}

	compute_maxrows();
}

/* Update any lines between old_current and current that need to be
 * updated.  Use this if we've moved without changing any text. */
void edit_redraw(filestruct *old_current, size_t pww_save)
{
	bool do_redraw = need_vertical_update(0) || need_vertical_update(pww_save);
	filestruct *foo = NULL;

	/* If either old_current or current is offscreen, scroll the edit
	 * window until it's onscreen and get out. */
	if (old_current->lineno < openfile->edittop->lineno ||
	        old_current->lineno >= openfile->edittop->lineno +
	        maxrows || openfile->current->lineno <
	        openfile->edittop->lineno || openfile->current->lineno >=
	        openfile->edittop->lineno + maxrows) {

		DEBUG_LOG("edit_redraw(): line " << openfile->current->lineno << " was offscreen, oldcurrent = " << old_current->lineno << " edittop = " << openfile->edittop->lineno);

		/* If the mark is on, update all the lines between old_current
		 * and either the old first line or old last line (depending on
		 * whether we've scrolled up or down) of the edit window. */
		if (openfile->mark_set) {
			ssize_t old_lineno;
			filestruct *old_edittop = openfile->edittop;

			if (old_edittop->lineno < openfile->edittop->lineno) {
				old_lineno = old_edittop->lineno;
			} else
				old_lineno = (old_edittop->lineno + maxrows <= openfile->filebot->lineno) ? old_edittop->lineno + editwinrows : openfile->filebot->lineno;

			foo = old_current;

			while (foo->lineno != old_lineno) {
				update_line(foo, 0);

				foo = (foo->lineno > old_lineno) ? foo->prev : foo->next;
			}
		}

		/* Make sure the current line is on the screen. */
		edit_update(CENTER);

		/* Update old_current if we're not on the same page as
		 * before. */
		if (do_redraw) {
			update_line(old_current, 0);
		}

		/* If the mark is on, update all the lines between the old first
		 * line or old last line of the edit window (depending on
		 * whether we've scrolled up or down) and current. */
		if (openfile->mark_set) {
			while (foo->lineno != openfile->current->lineno) {
				update_line(foo, 0);

				foo = (foo->lineno > openfile->current->lineno) ? foo->prev : foo->next;
			}
		}

		return;
	}

	/* Update old_current and current if we're not on the same page as
	 * before.  If the mark is on, update all the lines between
	 * old_current and current too. */
	foo = old_current;

	while (foo != openfile->current) {
		if (do_redraw) {
			update_line(foo, 0);
		}

		if (!openfile->mark_set) {
			break;
		}

		foo = (foo->lineno > openfile->current->lineno) ? foo->prev : foo->next;
	}

	if (do_redraw) {
		update_line(openfile->current, openfile->current_x);
	}
}

/* Refresh the screen without changing the position of lines.  Use this
 * if we've moved and changed text. */
void edit_refresh(void)
{
	filestruct *foo;
	int nlines;

	/* Figure out what maxrows should really be */
	compute_maxrows();

	if (openfile->current->lineno < openfile->edittop->lineno || openfile->current->lineno >= openfile->edittop->lineno + maxrows) {

		DEBUG_LOG("edit_refresh(): line = " << openfile->current->lineno << ", edittop " << openfile->edittop->lineno << " + maxrows " << maxrows);

		/* Make sure the current line is on the screen. */
		edit_update(ISSET(SMOOTH_SCROLL) ? NONE : CENTER);
	}

	foo = openfile->edittop;

	DEBUG_LOG("edit_refresh(): edittop->lineno = " << openfile->edittop->lineno);

	for (nlines = 0; nlines < editwinrows && foo != NULL; nlines++) {
		nlines += update_line(foo, (foo == openfile->current) ? openfile->current_x : 0);
		foo = foo->next;
	}

	for (; nlines < editwinrows; nlines++) {
		blank_line(edit, nlines, 0, COLS);
	}

	reset_cursor();
	wnoutrefresh(edit);
}

/* Move edittop to put it in range of current, keeping current in the
 * same place.  location determines how we move it: if it's CENTER, we
 * center current, and if it's NONE, we put current current_y lines
 * below edittop. */
void edit_update(UpdateType location)
{
	filestruct *foo = openfile->current;
	int goal;

	/* If location is CENTER, we move edittop up (editwinrows / 2)
	 * lines.  This puts current at the center of the screen.  If
	 * location is NONE, we move edittop up current_y lines if current_y
	 * is in range of the screen, 0 lines if current_y is less than 0,
	 * or (editwinrows - 1) lines if current_y is greater than
	 * (editwinrows - 1).  This puts current at the same place on the
	 * screen as before, or at the top or bottom of the screen if
	 * edittop is beyond either. */
	if (location == CENTER) {
		goal = editwinrows / 2;
	} else {
		goal = openfile->current_y;

		/* Limit goal to (editwinrows - 1) lines maximum. */
		if (goal > editwinrows - 1) {
			goal = editwinrows - 1;
		}
	}

	for (; goal > 0 && foo->prev != NULL; goal--) {
		foo = foo->prev;
		if (ISSET(SOFTWRAP) && foo) {
			goal -= strlenpt(foo->data) / COLS;
		}
	}
	openfile->edittop = foo;
	DEBUG_LOG("edit_udpate(), setting edittop to lineno " << openfile->edittop->lineno);
	compute_maxrows();
	edit_refresh_needed = true;
}

/* Unconditionally redraw the entire screen. */
void total_redraw(void)
{
#ifdef USE_SLANG
	/* Slang curses emulation brain damage, part 4: Slang doesn't define
	 * curscr. */
	SLsmg_touch_screen();
	SLsmg_refresh();
#else
	wrefresh(curscr);
#endif
}

/* Unconditionally redraw the entire screen, and then refresh it using
 * the current file. */
void total_refresh(void)
{
	total_redraw();
	titlebar(NULL);
	edit_refresh();
	bottombars(currmenu);
}

/* Display the main shortcut list on the last two rows of the bottom
 * portion of the window. */
void display_main_list(void)
{
	if (openfile->syntax && (openfile->syntax->formatter != "" || openfile->syntax->linter != "")) {
		set_lint_or_format_shortcuts();
	} else {
		set_spell_shortcuts();
	}
	bottombars(MMAIN);
}

/* If constant is true, we display the current cursor position only if
 * disable_cursorpos is false.  Otherwise, we display it
 * unconditionally and set disable_cursorpos to false.  If constant is
 * true and disable_cursorpos is true, we also set disable_cursorpos to
 * false, so that we leave the current statusbar alone this time, and
 * display the current cursor position next time. */
void do_cursorpos(bool constant)
{
	filestruct *f;
	char c;
	size_t i, cur_xpt = xplustabs() + 1;
	size_t cur_lenpt = strlenpt(openfile->current->data) + 1;
	int linepct, colpct, charpct;

	assert(openfile->fileage != NULL && openfile->current != NULL);

	f = openfile->current->next;
	c = openfile->current->data[openfile->current_x];

	openfile->current->next = NULL;
	openfile->current->data[openfile->current_x] = '\0';

	i = get_totsize(openfile->fileage, openfile->current);

	openfile->current->data[openfile->current_x] = c;
	openfile->current->next = f;

	if (constant && disable_cursorpos) {
		disable_cursorpos = false;
		return;
	}

	/* Display the current cursor position on the statusbar, and set
	 * disable_cursorpos to false. */
	linepct = 100 * openfile->current->lineno / openfile->filebot->lineno;
	colpct = 100 * cur_xpt / cur_lenpt;
	charpct = (openfile->totsize == 0) ? 0 : 100 * i / openfile->totsize;

	statusbar(
	    _("line %ld/%ld (%d%%), col %lu/%lu (%d%%), char %lu/%lu (%d%%)"),
	    (long)openfile->current->lineno,
	    (long)openfile->filebot->lineno, linepct,
	    (unsigned long)cur_xpt, (unsigned long)cur_lenpt, colpct,
	    (unsigned long)i, (unsigned long)openfile->totsize, charpct);

	disable_cursorpos = false;
}

/* Unconditionally display the current cursor position. */
void do_cursorpos_void(void)
{
	do_cursorpos(false);
}

void enable_nodelay(void)
{
	nodelay_mode = true;
	nodelay(edit, true);
}

void disable_nodelay(void)
{
	nodelay_mode = false;
	nodelay(edit, false);
}


/* Highlight the current word being replaced or spell checked.  We
 * expect word to have tabs and control characters expanded. */
void do_replace_highlight(bool highlight, const char *word)
{
	size_t y = xplustabs(), word_len = strlenpt(word);

	y = get_page_start(y) + COLS - y;
	/* Now y is the number of columns that we can display on this
	 * line. */

	assert(y > 0);

	if (word_len > y) {
		y--;
	}

	reset_cursor();
	wnoutrefresh(edit);

	if (highlight) {
		wattron(edit, highlight_attribute);
	}

	/* This is so we can show zero-length matches. */
	if (word_len == 0) {
		waddch(edit, ' ');
	} else {
		waddnstr(edit, word, actual_x(word, y));
	}

	if (word_len > y) {
		waddch(edit, '$');
	}

	if (highlight) {
		wattroff(edit, highlight_attribute);
	}
}

void set_color(WINDOW *win, ColorPair color)
{
	if (color.bright) {
		wattron(win, A_BOLD);
	}
	if (color.underline) {
		wattron(win, A_UNDERLINE);
	}
	wattron(win, color.pairnum);
}

void clear_color(WINDOW *win, ColorPair color)
{
	wattroff(win, A_UNDERLINE);
	wattroff(win, A_BOLD);
	wattroff(win, color.pairnum);
}
