/**************************************************************************
 *   browser.c                                                            *
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
#include "cpputil.h"

#include <algorithm>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

std::vector<std::string> filelist;
/* The list of files to display in the file browser. */
static int width = 0;
/* The number of files that we can display per line. */
static int longest = 0;
/* The number of columns in the longest filename in the list. */
static size_t selected = 0;
/* The currently selected filename in the list.  This variable
 * is zero-based. */
static bool search_last_file = false;
/* Have we gone past the last file while searching? */

/* Our main file browser function.  path is the tilde-expanded path we
 * start browsing from. */
std::string do_browser(std::string path, DIR *dir)
{
	std::string retval;
	bool old_const_update = ISSET(CONST_UPDATE);
	std::string prev_dir;
	/* The directory we were in, if any, before backing up via browsing to "..". */
	std::string ans;
	/* The last answer the user typed at the statusbar prompt. */
	size_t old_selected;
	/* The selected file we had before the current selected file. */
	const sc *s;
	const subnfunc *f;

	curs_set(0);
	blank_statusbar();
	currmenu = MBROWSER;
	bottombars(MBROWSER);
	wnoutrefresh(bottomwin);

	UNSET(CONST_UPDATE);

change_browser_directory:
	/* We go here after we select a new directory. */

	path = get_full_path(path);

	assert(path.length() > 0 && path.back() == '/');

	/* Get the file list, and set longest and width in the process. */
	browser_init(path, dir);

	/* Sort the file list. */
	std::sort(filelist.begin(), filelist.end(), sort_directories);

	/* If prev_dir isn't empty, select the directory saved in it, and then blow it away. */
	if (prev_dir != "") {
		browser_select_filename(prev_dir);

		prev_dir = "";
		/* Otherwise, select the first file or directory in the list. */
	} else {
		selected = 0;
	}

	old_selected = (size_t)-1;

	titlebar(path);

	void (*func)() = nullptr;
	Key *kbinput = nullptr;

	while (true) {
		struct stat st;
		size_t fileline = selected / width;
		/* The line number the selected file is on. */
		std::string new_path;
		/* The path we switch to at the "Go to Directory" prompt. */

		if (kbinput) {
			delete kbinput;
			kbinput = nullptr;
		}

		/* Display the file list if we don't have a key, or if the
		 * selected file has changed, and set width in the process. */
		if (old_selected != selected) {
			browser_refresh();
		}

		old_selected = selected;

		if (!func) {
			// Deal with the keyboard input
			kbinput = new Key(get_kbinput(edit));

			s = get_shortcut(*kbinput);
			if (!s) {
				continue;
			}
			f = sctofunc((sc *) s);
			if (!f) {
				break;
			}
			func = f->scfunc;
		}

		if (func == total_refresh) {
			total_redraw();
		} else if (func == do_help_void) {
			do_help_void();
			curs_set(0);
			/* Search for a filename. */
		} else if (func == do_search) {
			curs_set(1);
			do_filesearch();
			curs_set(0);
			/* Search for another filename. */
		} else if (func == do_research) {
			do_fileresearch();
		} else if (func == do_page_up) {
			if (selected >= (editwinrows + fileline % editwinrows) * width) {
				selected -= (editwinrows + fileline % editwinrows) * width;
			} else {
				selected = 0;
			}
		} else if (func == do_page_down) {
			selected += (editwinrows - fileline % editwinrows) * width;
			if (selected > filelist.size() - 1) {
				selected = filelist.size() - 1;
			}
		} else if (func == do_first_file) {
			selected = 0;
		} else if (func == do_last_file) {
			selected = filelist.size() - 1;
		} else if (func == goto_dir_void) {
			/* Go to a specific directory. */
			curs_set(1);

			std::shared_ptr<Key> key;
			PromptResult i = do_prompt(true,
			              false,
			              MGOTODIR, key, ans.c_str(),
			              NULL,
			              browser_refresh, _("Go To Directory"));

			curs_set(0);
			currmenu = MBROWSER;
			bottombars(MBROWSER);

			/* If the directory begins with a newline (i.e. an
			 * encoded null), treat it as though it's blank. */
			if (i == PROMPT_ABORTED || i == PROMPT_BLANK_STRING || answer.front() == '\n') {
				/* We canceled.  Indicate that on the statusbar, and
				 * blank out ans, since we're done with it. */
				statusbar(_("Cancelled"));
				ans = "";
				func = nullptr;
				continue;
			} else if (i != PROMPT_ENTER_PRESSED) {
				/* Put back the "Go to Directory" key and save
				 * answer in ans, so that the file list is displayed
				 * again, the prompt is displayed again, and what we
				 * typed before at the prompt is displayed again. */
				ans = answer;
				func = goto_dir_void;
				continue;
			}

			/* We have a directory.  Blank out ans, since we're done with it. */
			ans = "";

			/* Convert newlines to nulls, just before we go to the directory. */
			sunder(answer);

			new_path = real_dir_from_tilde(answer);

			if (new_path == "") {
				new_path = path + answer;
			}

			dir = opendir(new_path);
			if (dir == NULL) {
				/* We can't open this directory for some reason. Complain. */
				statusbar(_("Error reading %s: %s"), answer.c_str(), strerror(errno));
				beep();
				func = nullptr;
				continue;
			}

			/* Start over again with the new path value. */
			path = new_path;
			goto change_browser_directory;
		} else if (func == do_up_void) {
			if (selected >= width) {
				selected -= width;
			}
		} else if (func == do_left) {
			if (selected > 0) {
				selected--;
			}
		} else if (func == do_down_void) {
			if (selected + width <= filelist.size() - 1) {
				selected += width;
			}
		} else if (func == do_right) {
			if (selected < filelist.size() - 1) {
				selected++;
			}
		} else if (func == do_enter_void) {
			/* We can't move up from "/". */
			if (filelist[selected] == "/..") {
				statusbar(_("Can't move up a directory"));
				beep();
				func = nullptr;
				continue;
			}

			if (stat(filelist[selected], &st) == -1) {
				/* We can't open this file for some reason. Complain. */
				statusbar(_("Error reading %s: %s"), filelist[selected].c_str(), strerror(errno));
				beep();
				func = nullptr;
				continue;
			}

			if (!S_ISDIR(st.st_mode)) {
				/* We've successfully opened a file, we're done, so get out. */
				retval = filelist[selected];
				func = nullptr;
				break;
			} else if (tail(filelist[selected]) == "..") {
				/* We've successfully opened the parent directory,
				 * save the current directory in prev_dir, so that
				  * we can easily return to it by hitting Enter. */
				prev_dir = striponedir(filelist[selected]);
			}

			dir = opendir(filelist[selected].c_str());
			if (dir == NULL) {
				/* We can't open this directory for some reason. Complain. */
				statusbar(_("Error reading %s: %s"), filelist[selected].c_str(), strerror(errno));
				beep();
				func = nullptr;
				continue;
			}

			path = filelist[selected];

			/* Start over again with the new path value. */
			goto change_browser_directory;
		} else if (func == do_exit) {
			/* Exit from the file browser. */
			break;
		}
		func = nullptr;
	}
	if (kbinput) {
		delete kbinput;
	}
	titlebar(NULL);
	edit_refresh();
	curs_set(1);
	if (old_const_update) {
		SET(CONST_UPDATE);
	}

	filelist.clear();

	return retval;
}

/* The file browser front end.  We check to see if inpath has a
 * directory in it.  If it does, we start do_browser() from there.
 * Otherwise, we start do_browser() from the current directory. */
std::string do_browse_from(const std::string& inpath)
{
	struct stat st;
	DIR *dir = NULL;

	std::string path = real_dir_from_tilde(inpath);

	/* Perhaps path is a directory.  If so, we'll pass it to
	 * do_browser().  Or perhaps path is a directory / a file.  If so,
	 * we'll try stripping off the last path element and passing it to
	 * do_browser().  Or perhaps path doesn't have a directory portion
	 * at all.  If so, we'll just pass the current directory to
	 * do_browser(). */
	if (stat(path, &st) == -1 || !S_ISDIR(st.st_mode)) {
		path = striponedir(path);

		if (stat(path, &st) == -1 || !S_ISDIR(st.st_mode)) {
			path = getcwd();
		}
	}

	if (path != "") {
		dir = opendir(path.c_str());
	}

	/* If we can't open the path, get out. */
	if (dir == NULL) {
		beep();
		return NULL;
	}

	return do_browser(path, dir);
}

/* Set filelist to the list of files contained in the directory path,
 * set longest to the width in columns of the longest filename in that
 * list (between 15 and COLS), and set width to the number of files that
 * we can display per line.  longest needs to be at least 15 columns in
 * order to display ".. (parent dir)", as Pico does.  Assume path exists
 * and is a directory. */
void browser_init(const std::string& path, DIR *dir)
{
	const struct dirent *nextdir;
	int col = 0;
	/* The maximum number of columns that the filenames will take
	 * up. */
	int line = 0;
	/* The maximum number of lines that the filenames will take
	 * up. */
	int filesperline = 0;
	/* The number of files that we can display per line. */

	assert(path.length() > 0 && path.back() == '/' && dir != NULL);

	/* Set longest to zero, just before we initialize it. */
	longest = 0;

	while ((nextdir = readdir(dir)) != NULL) {
		size_t d_len;

		/* Don't show the "." entry. */
		if (strcmp(nextdir->d_name, ".") == 0) {
			continue;
		}

		d_len = strlenpt(nextdir->d_name);
		if (d_len > longest) {
			longest = (d_len > COLS) ? COLS : d_len;
		}
	}

	rewinddir(dir);

	/* Put 10 columns' worth of blank space between columns of filenames
	 * in the list whenever possible, as Pico does. */
	longest += 10;

	filelist.clear();

	while ((nextdir = readdir(dir)) != NULL) {
		/* Don't show the "." entry. */
		if (strcmp(nextdir->d_name, ".") == 0) {
			continue;
		}

		filelist.push_back(std::string(path) + std::string(nextdir->d_name));
	}

	closedir(dir);

	/* Make sure longest is between 15 and COLS. */
	if (longest < 15) {
		longest = 15;
	}
	if (longest > COLS) {
		longest = COLS;
	}

	/* Set width to zero, just before we initialize it. */
	width = 0;

	for (size_t i = 0; i < filelist.size() && line < editwinrows; i++) {
		/* Calculate the number of columns one filename will take up. */
		col += longest;
		filesperline++;

		/* Add some space between the columns. */
		col += 2;

		/* If the next entry isn't going to fit on the current line,
		 * move to the next line. */
		if (col > COLS - longest) {
			line++;
			col = 0;

			/* If width isn't initialized yet, and we've taken up more
			 * than one line, it means that width is equal to
			 * filesperline. */
			if (width == 0) {
				width = filesperline;
			}
		}
	}

	/* If width isn't initialized yet, and we've taken up only one line,
	 * it means that width is equal to longest. */
	if (width == 0) {
		width = longest;
	}
}

/* Set width to the number of files that we can display per line, if
 * necessary, and display the list of files. */
void browser_refresh(void)
{
	static int uimax_digits = -1;
	size_t i;
	int col = 0;
	/* The maximum number of columns that the filenames will take up. */
	int line = 0;
	/* The maximum number of lines that the filenames will take up. */
	std::string foo;
	/* The file information that we'll display. */

	if (uimax_digits == -1) {
		uimax_digits = digits(UINT_MAX);
	}

	blank_edit();

	wmove(edit, 0, 0);

	i = width * editwinrows * ((selected / width) / editwinrows);

	for (; i < filelist.size() && line < editwinrows; i++) {
		struct stat st;
		std::string filetail = tail(filelist[i]);
		/* The filename we display, minus the path. */
		int foomaxlen = 7;
		/* The maximum length of the file information in
		 * columns: seven for "--", "(dir)", or the file size,
		 * and 12 for "(parent dir)". */
		bool dots = (COLS >= 15 && filetail.length() >= longest - foomaxlen - 1);
		/* Do we put an ellipsis before the filename?  Don't set
		 * this to true if we have fewer than 15 columns (i.e.
		 * one column for padding, plus seven columns for a
		 * filename other than ".."). */
		std::string disp = display_string(filetail, dots ? filetail.length() - longest + foomaxlen + 4 : 0, longest, false);
		/* If we put an ellipsis before the filename, reserve one column
		 * for padding, plus seven columns for "--", "(dir)", or the file
		 * size, plus three columns for the ellipsis. */

		/* Start highlighting the currently selected file or
		 * directory. */
		if (i == selected) {
			wattron(edit, highlight_attribute);
		}

		blank_line(edit, line, col, longest);

		/* If dots is true, we will display something like "...ename". */
		if (dots) {
			mvwaddstr(edit, line, col, "...");
		}
		mvwaddstr(edit, line, dots ? col + 3 : col, disp.c_str());

		col += longest;

		/* Show information about the file.  We don't want to report
		 * file sizes for links, so we use lstat(). */
		if (lstat(filelist[i], &st) == -1 || S_ISLNK(st.st_mode)) {
			/* If the file doesn't exist (i.e. it's been deleted while
			 * the file browser is open), or it's a symlink that doesn't
			 * point to a directory, display "--". */
			if (stat(filelist[i], &st) == -1 || !S_ISDIR(st.st_mode)) {
				foo = "--";
			} else {
			/* If the file is a symlink that points to a directory,
			 * display it as a directory. */
			/* TRANSLATORS: Try to keep this at most 7
			 * characters. */
				foo = _("(dir)");
			}
		} else if (S_ISDIR(st.st_mode)) {
			/* If the file is a directory, display it as such. */
			if (filetail == "..") {
				/* TRANSLATORS: Try to keep this at most 12 characters. */
				foo = _("(parent dir)");
				foomaxlen = 12;
			} else {
				foo = _("(dir)");
			}
		} else {
			unsigned long result = st.st_size;
			char modifier;

			/* Bytes. */
			if (st.st_size < (1 << 10)) {
				modifier = ' ';
			}
			/* Kilobytes. */
			else if (st.st_size < (1 << 20)) {
				result >>= 10;
				modifier = 'K';
				/* Megabytes. */
			} else if (st.st_size < (1 << 30)) {
				result >>= 20;
				modifier = 'M';
				/* Gigabytes. */
			} else {
				result >>= 30;
				modifier = 'G';
			}

			char disp[8];
			snprintf(disp, 8, "%4lu %cB", result, modifier);
			foo = disp;
		}

		/* Make sure foo takes up no more than foomaxlen columns. */
		if (foo.length() > foomaxlen) {
			foo = foo.substr(0, foomaxlen);
		}

		mvwaddstr(edit, line, col - foo.length(), foo.c_str());

		/* Finish highlighting the currently selected file or directory. */
		if (i == selected) {
			wattroff(edit, highlight_attribute);
		}

		/* Add some space between the columns. */
		col += 2;

		/* If the next entry isn't going to fit on the current line,
		 * move to the next line. */
		if (col > COLS - longest) {
			line++;
			col = 0;
		}

		wmove(edit, line, col);
	}

	wnoutrefresh(edit);
}

/* Look for needle.  If we find it, set selected to its location.  Note
 * that needle must be an exact match for a file in the list.  The
 * return value specifies whether we found anything. */
bool browser_select_filename(const std::string& needle)
{
	size_t currselected;
	bool found = false;

	for (currselected = 0; currselected < filelist.size(); currselected++) {
		if (filelist[currselected] == needle) {
			found = true;
			break;
		}
	}

	if (found) {
		selected = currselected;
	}

	return found;
}

/* Set up the system variables for a filename search.  Return -1 if the
 * search should be canceled (due to Cancel, a blank search string, or a
 * failed regcomp()), return 0 on success, and return 1 on rerun calling
 * program. */
int filesearch_init(void)
{
	std::string buf;
	const sc *s;
	static std::string backupstring = "";
	/* The search string we'll be using. */

	/* We display the search prompt below.  If the user types a partial
	 * search string and then Replace or a toggle, we will return to
	 * do_search() or do_replace() and be called again.  In that case,
	 * we should put the same search string back up. */

	if (last_search != "") {
		std::string disp = display_string(last_search, 0, COLS / 3, false);

		buf = " [" + disp + ((last_search.length() > COLS / 3) ? "..." : "") + "]";
		/* We use (COLS / 3) here because we need to see more on the line. */
	} else {
		buf = "";
	}

	/* This is now one simple call.  It just does a lot. */
	std::shared_ptr<Key> key;
	PromptResult i = do_prompt(false,
	              true,
	              MWHEREISFILE, key, backupstring.c_str(),
	              &search_history,
	              browser_refresh, "%s%s%s%s%s", _("Search"),
	              /* This string is just a modifier for the search prompt; no
	               * grammar is implied. */
	              ISSET(CASE_SENSITIVE) ? _(" [Case Sensitive]") : "",
	              /* This string is just a modifier for the search prompt; no
	               * grammar is implied. */
	              ISSET(USE_REGEXP) ? _(" [Regexp]") : "",
	              /* This string is just a modifier for the search prompt; no
	               * grammar is implied. */
	              ISSET(BACKWARDS_SEARCH) ? _(" [Backwards]") : "",
								buf.c_str());


	backupstring = "";

	/* Cancel any search, or just return with no previous search. */
	if (i == PROMPT_ABORTED || (i == PROMPT_BLANK_STRING && last_search == "") || (i == PROMPT_ENTER_PRESSED && answer == "")) {
		statusbar(_("Cancelled"));
		return -1;
	} else {
		s = get_shortcut(*key);
		if (i == PROMPT_BLANK_STRING || i == PROMPT_ENTER_PRESSED) {
			/* Use last_search if answer is an empty string, or answer if it isn't. */
			if (ISSET(USE_REGEXP) && !regexp_init((i == PROMPT_BLANK_STRING) ? last_search.c_str() : answer.c_str())) {
				return -1;
			}
		} else if (s && s->scfunc == case_sens_void) {
			TOGGLE(CASE_SENSITIVE);
			backupstring = answer;
			return 1;
		} else if (s && s->scfunc ==  backwards_void) {
			TOGGLE(BACKWARDS_SEARCH);
			backupstring = answer;
			return 1;
		} else {
			if (s && s->scfunc == regexp_void) {
				TOGGLE(USE_REGEXP);
				backupstring = answer;
				return 1;
			} else {
				return -1;
			}
		}
	}

	return 0;
}

/* Look for needle.  If no_sameline is true, skip over selected when
 * looking for needle.  begin is the location of the filename where we
 * first started searching.  The return value specifies whether we found
 * anything. */
bool findnextfile(bool no_sameline, size_t begin, const std::string& needle)
{
	size_t currselected = selected;
	/* The location in the current file list of the match we find. */
	std::string filetail = tail(filelist[currselected]);
	/* The filename we display, minus the path. */
	const char *rev_start = filetail.c_str(), *found = NULL;

	if (ISSET(BACKWARDS_SEARCH)) {
		rev_start += strlen(rev_start);
	}

	/* Look for needle in the current filename we're searching. */
	while (true) {
		found = strstrwrapper(filetail.c_str(), needle.c_str(), rev_start);

		/* We've found a potential match.  If we're not allowed to find
		 * a match on the same filename we started on and this potential
		 * match is on that line, continue searching. */
		if (found != NULL && (!no_sameline || currselected != begin)) {
			break;
		}

		/* We've finished processing the filenames, so get out. */
		if (search_last_file) {
			not_found_msg(needle);
			return false;
		}

		/* Move to the previous or next filename in the list.  If we've
		 * reached the start or end of the list, wrap around. */
		if (ISSET(BACKWARDS_SEARCH)) {
			if (currselected > 0) {
				currselected--;
			} else {
				currselected = filelist.size() - 1;
				statusbar(_("Search Wrapped"));
			}
		} else {
			if (currselected < filelist.size() - 1) {
				currselected++;
			} else {
				currselected = 0;
				statusbar(_("Search Wrapped"));
			}
		}

		/* We've reached the original starting file. */
		if (currselected == begin) {
			search_last_file = true;
		}

		filetail = tail(filelist[currselected]);

		rev_start = filetail.c_str();
		if (ISSET(BACKWARDS_SEARCH)) {
			rev_start += strlen(rev_start);
		}
	}

	/* We've definitely found something. */
	selected = currselected;

	return true;
}

/* Clear the flag indicating that a search reached the last file in the
 * list.  We need to do this just before a new search. */
void findnextfile_wrap_reset(void)
{
	search_last_file = false;
}

/* Abort the current filename search.  Clean up by setting the current
 * shortcut list to the browser shortcut list, displaying it, and
 * decompiling the compiled regular expression we used in the last
 * search, if any. */
void filesearch_abort(void)
{
	currmenu = MBROWSER;
	bottombars(MBROWSER);
	regexp_cleanup();
}

/* Search for a filename. */
void do_filesearch(void)
{
	size_t begin = selected;
	int i;
	bool didfind;

	i = filesearch_init();
	if (i == -1) {
		/* Cancel, blank search string, or regcomp() failed. */
		filesearch_abort();
	} else if (i == 1) {
		/* Case Sensitive, Backwards, or Regexp search toggle. */
		do_filesearch();
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

	findnextfile_wrap_reset();
	didfind = findnextfile(false, begin, answer);

	/* Check to see if there's only one occurrence of the string and we're on it now. */
	if (selected == begin && didfind) {
		/* Do the search again, skipping over the current line.  We
		 * should only end up back at the same position if the string
		 * isn't found again, in which case it's the only occurrence. */
		didfind = findnextfile(true, begin, answer);
		if (selected == begin && !didfind) {
			statusbar(_("This is the only occurrence"));
		}
	}

	filesearch_abort();
}

/* Search for the last filename without prompting. */
void do_fileresearch(void)
{
	size_t begin = selected;
	bool didfind;

	if (last_search != "") {
		/* Since answer is "", use last_search! */
		if (ISSET(USE_REGEXP) && !regexp_init(last_search.c_str())) {
			return;
		}

		findnextfile_wrap_reset();
		didfind = findnextfile(false, begin, answer);

		/* Check to see if there's only one occurrence of the string and
		 * we're on it now. */
		if (selected == begin && didfind) {
			/* Do the search again, skipping over the current line.  We
			 * should only end up back at the same position if the
			 * string isn't found again, in which case it's the only
			 * occurrence. */
			didfind = findnextfile(true, begin, answer);
			if (selected == begin && !didfind) {
				statusbar(_("This is the only occurrence"));
			}
		}
	} else {
		statusbar(_("No current search pattern"));
	}

	filesearch_abort();
}

/* Select the first file in the list. */
void do_first_file(void)
{
	selected = 0;
}

/* Select the last file in the list. */
void do_last_file(void)
{
	selected = filelist.size() - 1;
}

/* Strip one directory from the end of path, and return the stripped path. */
std::string striponedir(const std::string& path)
{
	auto pos = path.rfind("/");
	if (pos == std::string::npos) {
		return path;
	}
	return path.substr(0, pos);
}
