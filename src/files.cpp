/**************************************************************************
 *   files.c                                                              *
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
#include <fstream>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <pwd.h>

/* Add an entry to the list of open files. This should only be called from open_buffer(). */
void make_new_buffer(void)
{
	OpenFile newfile;
	if (openfiles.size() == 0) {
		/* If there are no entries in openfile, make the first one and move to it. */
		openfiles.push_back(newfile);
		openfile = openfiles.begin();
	} else {
		/* Otherwise, make a new entry for openfile, splice it in after the current entry, and move to it. */
		openfiles.insert(std::next(openfile, 1), newfile);
		++openfile;
	}

	/* Initialize the new buffer. */
	initialize_buffer();
}

/* Initialize the current entry of the openfiles list */
void initialize_buffer(void)
{
	assert(openfile != openfiles.end());

	openfile->filename = mallocstrcpy(NULL, "");

	initialize_buffer_text();

	openfile->current_x = 0;
	openfile->placewewant = 0;
	openfile->current_y = 0;

	openfile->modified = false;
	openfile->mark_set = false;

	openfile->mark_begin = NULL;
	openfile->mark_begin_x = 0;

	openfile->fmt = NIX_FILE;

	openfile->current_stat = nullptr;
	openfile->undotop = NULL;
	openfile->current_undo = NULL;
}

/* Initialize the text of the current entry of the openfiles list */
void initialize_buffer_text(void)
{
	assert(openfile != openfiles.end());

	openfile->fileage = make_new_node(NULL);
	openfile->fileage->data = mallocstrcpy(NULL, "");

	openfile->filebot = openfile->fileage;
	openfile->edittop = openfile->fileage;
	openfile->current = openfile->fileage;

	openfile->fileage->multidata = NULL;

	openfile->totsize = 0;
}

/* Actually write the lock file.  This function will
   ALWAYS annihilate any previous version of the file.
   We'll borrow INSECURE_BACKUP here to decide about lock file
   paranoia here as well...
   Args:
       lockfilename: file name for lock
       origfilename: name of the file the lock is for
       modified: whether to set the modified bit in the file

   Returns: 1 on success, 0 on failure (but continue loading), -1 on failure and abort
 */
int write_lockfile(const std::string& lockfilename, const std::string& origfilename, bool modified)
{
	int cflags, fd;
	FILE *filestream;
	pid_t mypid;
	uid_t myuid;
	struct passwd *mypwuid;
	char *lockdata = charalloc(1024);
	char myhostname[32];
	ssize_t lockdatalen = 1024;
	ssize_t wroteamt;

	/* Run things which might fail first before we try and blow away the old state */
	myuid = geteuid();
	if ((mypwuid = getpwuid(myuid)) == NULL) {
		statusbar(_("Couldn't determine my identity for lock file (getpwuid() failed)"));
		return -1;
	}
	mypid = getpid();

	if (gethostname(myhostname, 31) < 0) {
		statusbar(_("Couldn't determine hostname for lock file: %s"), strerror(errno));
		return -1;
	}

	if (delete_lockfile(lockfilename) < 0) {
		return -1;
	}

	if (ISSET(INSECURE_BACKUP)) {
		cflags = O_WRONLY | O_CREAT | O_APPEND;
	} else {
		cflags = O_WRONLY | O_CREAT | O_EXCL | O_APPEND;
	}

	fd = open(lockfilename.c_str(), cflags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

	/* Maybe we just don't have write access, don't stop us from
	   opening the file at all, just don't set the lock_filename
	   and return success */
	if (fd < 0 && errno == EACCES) {
		return 1;
	}

	/* Now we've got a safe file stream.  If the previous open()
	call failed, this will return NULL. */
	filestream = fdopen(fd, "wb");

	if (fd < 0 || filestream == NULL) {
		statusbar(_("Error writing lock file %s: %s"), lockfilename.c_str(), strerror(errno));
		return -1;
	}


	/* Okay. so at the moment we're following this state for how
	   to store the lock data:
	   byte 0        - 0x62
	   byte 1        - 0x30
	   bytes 2-12    - program name which created the lock
	   bytes 24,25   - little endian store of creator program's PID
	                   (b24 = 256^0 column, b25 = 256^1 column)
	   bytes 28-44   - username of who created the lock
	   bytes 68-100  - hostname of where the lock was created
	   bytes 108-876 - filename the lock is for
	   byte 1007     - 0x55 if file is modified

	   Looks like VIM also stores undo state in this file so we're
	   gonna have to figure out how to slap a 'OMG don't use recover
	   on our lockfile' message in here...

	   This is likely very wrong, so this is a WIP
	 */
	null_at(&lockdata, lockdatalen);
	lockdata[0] = 0x62;
	lockdata[1] = 0x30;
	lockdata[24] = mypid % 256;
	lockdata[25] = mypid / 256;
	snprintf(&lockdata[2], 10, "pinot%s", VERSION);
	strncpy(&lockdata[28], mypwuid->pw_name, 16);
	strncpy(&lockdata[68], myhostname, 31);
	strncpy(&lockdata[108], origfilename.c_str(), 768);
	if (modified == true) {
		lockdata[1007] = 0x55;
	}

	wroteamt = fwrite(lockdata, sizeof(char), lockdatalen, filestream);
	if (wroteamt < lockdatalen) {
		statusbar(_("Error writing lock file %s: %s"), lockfilename.c_str(), ferror(filestream));
		return -1;
	}

	DEBUG_LOG("In write_lockfile(), write successful (wrote " << wroteamt << " bytes)");

	if (fclose(filestream) == EOF) {
		statusbar(_("Error writing lock file %s: %s"), lockfilename.c_str(), strerror(errno));
		return -1;
	}

	openfile->lock_filename = lockfilename;

	return 1;
}


/* Less exciting, delete the lock file.
   Return -1 if successful and complain on the statusbar, 1 otherwite
 */
int delete_lockfile(const std::string& lockfilename)
{
	if (unlink(lockfilename.c_str()) < 0 && errno != ENOENT) {
		statusbar(_("Error deleting lock file %s: %s"), lockfilename.c_str(), strerror(errno));
		return -1;
	}
	return 1;
}


/* Deal with lockfiles.  Return -1 on refusing to override
   the lock file, and 1 on successfully created the lockfile, 0 means
   we were not successful on creating the lockfile but we should
   continue to load the file and complain to the user.
 */
int do_lockfile(const std::string& filename)
{
	std::string lockdir = dirname(filename);
	std::string lockbase = basename(filename.c_str());
	char lockprog[12], lockuser[16];
	struct stat fileinfo;
	int lockfd, lockpid;

	std::string lockfilename = lockdir + "/" + locking_prefix + lockbase + locking_suffix;
	DEBUG_LOG("lock file name is " << lockfilename);
	if (stat(lockfilename, &fileinfo) != -1) {
		ssize_t readtot = 0;
		ssize_t readamt = 0;
		char *lockbuf = (char *)nmalloc(8192);
		char *promptstr = (char *)nmalloc(128);
		int ans;
		if ((lockfd = open(lockfilename.c_str(), O_RDONLY)) < 0) {
			statusbar(_("Error opening lockfile %s: %s"), lockfilename.c_str(), strerror(errno));
			return -1;
		}
		do {
			readamt = read(lockfd, &lockbuf[readtot], BUFSIZ);
			readtot += readamt;
		} while (readtot < 8192 && readamt > 0);

		if (readtot < 48) {
			statusbar(_("Error reading lockfile %s: Not enough data read"), lockfilename.c_str());
			return -1;
		}
		strncpy(lockprog, &lockbuf[2], 10);
		lockpid = lockbuf[25] * 256 + lockbuf[24];
		strncpy(lockuser, &lockbuf[28], 16);
		DEBUG_LOG("lockpid = " << lockpid);
		DEBUG_LOG("program name which created this lock file should be " << lockprog);
		DEBUG_LOG("user which created this lock file should be " << lockuser);
		sprintf(promptstr, _("File is being edited (by %s, PID %d, user %s); continue?"), lockprog, lockpid, lockuser);
		ans = do_yesno_prompt(false, promptstr);
		if (ans < 1) {
			blank_statusbar();
			return -1;
		}
	} else {
		std::string lockfile_dir = dirname(lockfilename);
		if (stat(lockfile_dir.c_str(), &fileinfo) == -1) {
			statusbar(_("Error writing lock file: Directory \'%s\' doesn't exist"), lockfile_dir.c_str());
			return 0;
		}
	}

	return write_lockfile(lockfilename, filename, false);
}

/* If it's not "", filename is a file to open.  We make a new buffer, if
 * necessary, and then open and read the file, if applicable. */
void open_buffer(const std::string& filename, bool undoable)
{
	bool new_buffer = (openfiles.size() == 0 || ISSET(MULTIBUFFER));
	/* Whether we load into this buffer or a new one. */
	FILE *f;
	int rc;
	/* rc == -2 means that we have a new file.  -1 means that the
	 * open() failed.  0 means that the open() succeeded. */

	/* If we're loading into a new buffer, add a new entry to openfile. */
	if (new_buffer) {
		make_new_buffer();
	}

	/* If the filename isn't blank, open the file.  Otherwise, treat it
	 * as a new file. */
	rc = (filename != "") ? open_file(filename, new_buffer, &f) : -2;

	/* If we have a file, and we're loading into a new buffer, update
	 * the filename. */
	if (rc != -1 && new_buffer) {
		openfile->filename = filename;
	}

	/* If we have a non-new file, read it in.  Then, if the buffer has
	 * no stat, update the stat, if applicable. */
	if (rc > 0) {
		read_file(f, rc, filename, undoable, new_buffer);
		if (openfile->current_stat == nullptr) {
			openfile->current_stat = new struct stat;
			stat(filename, openfile->current_stat);
		}
	}

	/* If we have a file, and we're loading into a new buffer, move back
	 * to the beginning of the first line of the buffer. */
	if (rc != -1 && new_buffer) {
		openfile->current = openfile->fileage;
		openfile->current_x = 0;
		openfile->placewewant = 0;
	}

	/* If we're loading into a new buffer, update the colors to account
	 * for it, if applicable. */
	if (new_buffer) {
		color_update();
	}
}

#ifdef ENABLE_SPELLER
/* If it's not "", filename is a file to open.  We blow away the text of
 * the current buffer, and then open and read the file, if
 * applicable.  Note that we skip the operating directory test when
 * doing this. */
void replace_buffer(const std::string& filename)
{
	FILE *f;
	int rc;
	/* rc == -2 means that we have a new file.  -1 means that the
	 * open() failed.  0 means that the open() succeeded. */

	/* If the filename isn't blank, open the file.  Otherwise, treat it
	 * as a new file. */
	rc = (filename != "") ? open_file(filename, true, &f) : -2;

	/* Reinitialize the text of the current buffer. */
	free_filestruct(openfile->fileage);
	initialize_buffer_text();

	/* If we have a non-new file, read it in. */
	if (rc > 0) {
		read_file(f, rc, filename, false, true);
	}

	/* Move back to the beginning of the first line of the buffer. */
	openfile->current = openfile->fileage;
	openfile->current_x = 0;
	openfile->placewewant = 0;
}
#endif /* ENABLE_SPELLER */

/* Update the screen to account for the current buffer. */
void display_buffer(void)
{
	/* Update the titlebar, since the filename may have changed. */
	titlebar(NULL);

	/* Make sure we're using the buffer's associated colors, if
	 * applicable. */
	color_init();

	/* Update the edit window. */
	edit_refresh();
}

/* Switch to the next file buffer if next_buf is true.  Otherwise,
 * switch to the previous file buffer. */
std::list<OpenFile>::iterator switch_to_prevnext_buffer(bool next_buf)
{
	assert(openfiles.size() > 0);

	/* If only one file buffer is open, indicate it on the statusbar and get out. */
	if (openfiles.size() == 1) {
		statusbar(_("No more open file buffers"));
		return openfile;
	}

	/* Switch to the next or previous file buffer, depending on the
	 * value of next_buf. */
	auto oldfile = openfile;
	if (next_buf) {
		if (++openfile == openfiles.end()) {
			DEBUG_LOG("Reached end of list; looping to the beginning");
			openfile = openfiles.begin();
		}
	} else {
		if (openfile == openfiles.begin()) {
			DEBUG_LOG("Reached beginning of list; looping to the end");
			openfile = openfiles.end();
		}
		--openfile;
	}

	DEBUG_LOG("filename is " << openfile->filename);

	/* Update the screen to account for the current buffer. */
	display_buffer();

	/* Indicate the switch on the statusbar. */
	statusbar(_("Switched to %s"), ((openfile->filename[0] == '\0') ? _("New Buffer") : openfile->filename.c_str()));

#ifdef DEBUG
	dump_filestruct(openfile->current);
#endif

	display_main_list();

	return oldfile;
}

/* Switch to the previous entry in the openfile filebuffer. */
void switch_to_prev_buffer_void(void)
{
	switch_to_prevnext_buffer(false);
}

/* Switch to the next entry in the openfile filebuffer. */
void switch_to_next_buffer_void(void)
{
	switch_to_prevnext_buffer(true);
}

/* Delete an entry from the openfile filebuffer, and switch to the one
 * after it.  Return true on success, or false if there are no more open
 * file buffers. */
bool close_buffer(void)
{
	assert(openfiles.size() > 0);

	/* If only one file buffer is open, get out. */
	if (openfiles.size() == 1) {
		return false;
	}

	DEBUG_LOG("openfile == " << static_cast<void*>(&(*openfile)) << "; openfile->current_stat == " << static_cast<void*>(openfile->current_stat));

	update_poshistory(openfile->filename, openfile->current->lineno, xplustabs() + 1);

	/* Switch to the next file buffer. */
	auto oldfile = switch_to_prevnext_buffer(true);

	openfile = openfiles.erase(oldfile);
	if (openfile == openfiles.end()) {
		openfile = openfiles.begin();
	}

	display_main_list();

	return true;
}

/* A bit of a copy and paste from open_file(), is_file_writable()
 * just checks whether the file is appendable as a quick
 * permissions check, and we tend to err on the side of permissiveness
 * (reporting true when it might be wrong) to not fluster users
 * editing on odd filesystems by printing incorrect warnings.
 */
int is_file_writable(const std::string& filename)
{
	struct stat fileinfo, fileinfo2;
	int fd;
	FILE *f;
	bool ans = true;


	if (ISSET(VIEW_MODE)) {
		return true;
	}

	/* Get the specified file's full path. */
	auto full_filename = get_full_path(filename);

	/* Okay, if we can't stat the path due to a component's
	   permissions, just try the relative one */
	if (full_filename == "" || (stat(full_filename, &fileinfo) == -1 && stat(filename, &fileinfo2) != -1)) {
		full_filename = filename;
	}

	if ((fd = open(full_filename.c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) == -1 || (f = fdopen(fd, "a")) == NULL) {
		ans = false;
	} else {
		fclose(f);
	}
	close(fd);

	return ans;
}

/* We make a new line of text from buf.  buf is length buf_len.  If
 * first_line_ins is true, then we put the new line at the top of the
 * file.  Otherwise, we assume prevnode is the last line of the file,
 * and put our line after prevnode. */
filestruct *read_line(char *buf, filestruct *prevnode, bool *first_line_ins, size_t buf_len)
{
	filestruct *fileptr = (filestruct *)nmalloc(sizeof(filestruct));

	/* Convert nulls to newlines.  buf_len is the string's real length. */
	unsunder(buf, buf_len);

	assert(openfile->fileage != NULL && strlen(buf) == buf_len);

	fileptr->data = mallocstrcpy(NULL, buf);

	/* If it's a DOS file ("\r\n"), and file conversion isn't disabled,
	 * strip the '\r' part from fileptr->data. */
	if (!ISSET(NO_CONVERT) && buf_len > 0 && buf[buf_len - 1] == '\r') {
		fileptr->data[buf_len - 1] = '\0';
	}

	fileptr->multidata = NULL;

	if (*first_line_ins) {
		/* Special case: We're inserting with the cursor on the first line. */
		fileptr->prev = NULL;
		fileptr->next = openfile->fileage;
		fileptr->lineno = 1;
		if (*first_line_ins) {
			*first_line_ins = false;
			/* If we're inserting into the first line of the file, then
			 * we want to make sure that our edit buffer stays on the
			 * first line and that fileage stays up to date. */
			openfile->edittop = fileptr;
		} else {
			openfile->filebot = fileptr;
		}
		openfile->fileage = fileptr;
	} else {
		assert(prevnode != NULL);

		fileptr->prev = prevnode;
		fileptr->next = NULL;
		fileptr->lineno = prevnode->lineno + 1;
		prevnode->next = fileptr;
	}

	return fileptr;
}

/* Read an open file into the current buffer.  f should be set to the
 * open file, and filename should be set to the name of the file.
 * undoable  means do we want to create undo records to try and undo this.
 * Will also attempt to check file writability if fd > 0 and checkwritable == true
 */
void read_file(FILE *f, int fd, const std::string& filename, bool undoable, bool checkwritable)
{
	size_t num_lines = 0;
	/* The number of lines in the file. */
	size_t len = 0;
	/* The length of the current line of the file. */
	size_t i = 0;
	/* The position in the current line of the file. */
	size_t bufx = MAX_BUF_SIZE;
	/* The size of each chunk of the file that we read. */
	char input = '\0';
	/* The current input character. */
	char *buf;
	/* The buffer where we store chunks of the file. */
	filestruct *fileptr = openfile->current;
	/* The current line of the file. */
	bool first_line_ins = false;
	/* Whether we're inserting with the cursor on the first line. */
	int input_int;
	/* The current value we read from the file, whether an input
	 * character or EOF. */
	bool writable = true;
	/* Is the file writable (if we care) */
	int format = 0;
	/* 0 = *nix, 1 = DOS, 2 = Mac, 3 = both DOS and Mac. */

	assert(openfile->fileage != NULL && openfile->current != NULL);

	buf = charalloc(bufx);
	buf[0] = '\0';

	if (undoable) {
		add_undo(INSERT);
	}

	if (openfile->current == openfile->fileage) {
		first_line_ins = true;
	} else {
		fileptr = openfile->current->prev;
	}

	/* Read the entire file into the filestruct. */
	while ((input_int = getc(f)) != EOF) {
		input = (char)input_int;

		/* If it's a *nix file ("\n") or a DOS file ("\r\n"), and file
		 * conversion isn't disabled, handle it! */
		if (input == '\n') {
			/* If it's a DOS file or a DOS/Mac file ('\r' before '\n' on
			 * the first line if we think it's a *nix file, or on any
			 * line otherwise), and file conversion isn't disabled,
			 * handle it! */
			if (!ISSET(NO_CONVERT) && (num_lines == 0 || format != 0) && i > 0 && buf[i - 1] == '\r') {
				if (format == 0 || format == 2) {
					format++;
				}
			}

			/* Read in the line properly. */
			fileptr = read_line(buf, fileptr, &first_line_ins, len);

			/* Reset the line length in preparation for the next
			 * line. */
			len = 0;

			num_lines++;
			buf[0] = '\0';
			i = 0;
			/* If it's a Mac file ('\r' without '\n' on the first line if we
			 * think it's a *nix file, or on any line otherwise), and file
			 * conversion isn't disabled, handle it! */
		} else if (!ISSET(NO_CONVERT) && (num_lines == 0 || format != 0) && i > 0 && buf[i - 1] == '\r') {
			/* If we currently think the file is a *nix file, set format
			 * to Mac.  If we currently think the file is a DOS file,
			 * set format to both DOS and Mac. */
			if (format == 0 || format == 1) {
				format += 2;
			}

			/* Read in the line properly. */
			fileptr = read_line(buf, fileptr, &first_line_ins, len);

			/* Reset the line length in preparation for the next line.
			 * Since we've already read in the next character, reset it
			 * to 1 instead of 0. */
			len = 1;

			num_lines++;
			buf[0] = input;
			buf[1] = '\0';
			i = 1;
		} else {
			/* Calculate the total length of the line.  It might have
			 * nulls in it, so we can't just use strlen() here. */
			len++;

			/* Now we allocate a bigger buffer MAX_BUF_SIZE characters
			 * at a time.  If we allocate a lot of space for one line,
			 * we may indeed have to use a buffer this big later on, so
			 * we don't decrease it at all.  We do free it at the end,
			 * though. */
			if (i >= bufx - 1) {
				bufx += MAX_BUF_SIZE;
				buf = charealloc(buf, bufx);
			}

			buf[i] = input;
			buf[i + 1] = '\0';
			i++;
		}
	}

	/* Perhaps this could use some better handling. */
	if (ferror(f)) {
		nperror(filename.c_str());
	}
	fclose(f);
	if (fd > 0 && checkwritable) {
		close(fd);
		writable = is_file_writable(filename);
	}

	/* If file conversion isn't disabled and the last character in this
	 * file is '\r', read it in properly as a Mac format line. */
	if (len == 0 && !ISSET(NO_CONVERT) && input == '\r') {
		len = 1;

		buf[0] = input;
		buf[1] = '\0';
	}

	/* Did we not get a newline and still have stuff to do? */
	if (len > 0) {
		/* If file conversion isn't disabled and the last character in
		 * this file is '\r', set format to Mac if we currently think
		 * the file is a *nix file, or to both DOS and Mac if we
		 * currently think the file is a DOS file. */
		if (!ISSET(NO_CONVERT) && buf[len - 1] == '\r' && (format == 0 || format == 1)) {
			format += 2;
		}

		/* Read in the last line properly. */
		fileptr = read_line(buf, fileptr, &first_line_ins, len);
		num_lines++;
	}

	free(buf);

	/* If we didn't get a file and we don't already have one, open a blank buffer. */
	if (fileptr == NULL) {
		open_buffer("", false);
	}

	/* Attach the file we got to the filestruct.  If we got a file of
	 * zero bytes, don't do anything. */
	if (num_lines > 0) {
		/* If the file we got doesn't end in a newline, tack its last
		 * line onto the beginning of the line at current. */
		if (len > 0) {
			size_t current_len = strlen(openfile->current->data);

			/* Adjust the current x-coordinate to compensate for the
			 * change in the current line. */
			if (num_lines == 1) {
				openfile->current_x += len;
			} else {
				openfile->current_x = len;
			}

			/* Tack the text at fileptr onto the beginning of the text at current. */
			openfile->current->data = charealloc(openfile->current->data, len + current_len + 1);
			charmove(openfile->current->data + len, openfile->current->data, current_len + 1);
			strncpy(openfile->current->data, fileptr->data, len);

			/* Don't destroy fileage, edittop, or filebot! */
			if (fileptr == openfile->fileage) {
				openfile->fileage = openfile->current;
			}
			if (fileptr == openfile->edittop) {
				openfile->edittop = openfile->current;
			}
			if (fileptr == openfile->filebot) {
				openfile->filebot = openfile->current;
			}

			/* Move fileptr back one line and blow away the old fileptr,
			 * since its text has been saved. */
			fileptr = fileptr->prev;
			if (fileptr != NULL) {
				if (fileptr->next != NULL) {
					free(fileptr->next);
				}
			}
		}

		/* Attach the line at current after the line at fileptr. */
		if (fileptr != NULL) {
			fileptr->next = openfile->current;
			openfile->current->prev = fileptr;
		}

		/* Renumber starting with the last line of the file we inserted. */
		renumber(openfile->current);
	}

	openfile->totsize += get_totsize(openfile->fileage, openfile->filebot);

	/* If the NO_NEWLINES flag isn't set, and text has been added to
	 * the magicline (i.e. a file that doesn't end in a newline has been
	 * inserted at the end of the current buffer), add a new magicline,
	 * and move the current line down to it. */
	if (!ISSET(NO_NEWLINES) && openfile->filebot->data[0] != '\0') {
		new_magicline();
		openfile->current = openfile->filebot;
		openfile->current_x = 0;
	}

	/* Set the current place we want to the end of the last line of the
	 * file we inserted. */
	openfile->placewewant = xplustabs();

	if (undoable) {
		update_undo(INSERT);
	}

	if (format == 3) {
		if (writable)
			statusbar(
			    P_("Read %lu line (Converted from DOS and Mac format)",
			       "Read %lu lines (Converted from DOS and Mac format)",
			       (unsigned long)num_lines), (unsigned long)num_lines);
		else
			statusbar(
			    P_("Read %lu line (Converted from DOS and Mac format - Warning: No write permission)",
			       "Read %lu lines (Converted from DOS and Mac format - Warning: No write permission)",
			       (unsigned long)num_lines), (unsigned long)num_lines);
	} else if (format == 2) {
		openfile->fmt = MAC_FILE;
		if (writable)
			statusbar(P_("Read %lu line (Converted from Mac format)",
			             "Read %lu lines (Converted from Mac format)",
			             (unsigned long)num_lines), (unsigned long)num_lines);
		else
			statusbar(P_("Read %lu line (Converted from Mac format - Warning: No write permission)",
			             "Read %lu lines (Converted from Mac format - Warning: No write permission)",
			             (unsigned long)num_lines), (unsigned long)num_lines);
	} else if (format == 1) {
		openfile->fmt = DOS_FILE;
		if (writable)
			statusbar(P_("Read %lu line (Converted from DOS format)",
			             "Read %lu lines (Converted from DOS format)",
			             (unsigned long)num_lines), (unsigned long)num_lines);
		else
			statusbar(P_("Read %lu line (Converted from DOS format - Warning: No write permission)",
			             "Read %lu lines (Converted from DOS format - Warning: No write permission)",
			             (unsigned long)num_lines), (unsigned long)num_lines);
	} else if (writable)
		statusbar(P_("Read %lu line", "Read %lu lines",
		             (unsigned long)num_lines), (unsigned long)num_lines);
	else
		statusbar(P_("Read %lu line ( Warning: No write permission)",
		             "Read %lu lines (Warning: No write permission)",
		             (unsigned long)num_lines), (unsigned long)num_lines);
}

/* Open the file (and decide if it exists).  If newfie is true, display
 * "New File" if the file is missing.  Otherwise, say "[filename] not
 * found".
 *
 * Return -2 if we say "New File", -1 if the file isn't opened, and the
 * fd opened otherwise.  The file might still have an error while reading
 * with a 0 return value.  *f is set to the opened file. */
int open_file(const std::string& filename, bool newfie, FILE **f)
{
	struct stat fileinfo, fileinfo2;
	int fd;
	bool quiet = false;
	std::string full_filename;

	assert(f != NULL);

	/* Get the specified file's full path. */
	full_filename = get_full_path(filename);

	/* Okay, if we can't stat the path due to a component's
	   permissions, just try the relative one */
	if (full_filename == "" || (stat(full_filename, &fileinfo) == -1 && stat(filename, &fileinfo2) != -1)) {
		full_filename = filename;
	}

	if (ISSET(LOCKING)) {
		int lock_status = do_lockfile(full_filename);
		if (lock_status < 0) {
			return -1;
		} else if (lock_status == 0) {
			quiet = true;
		}
	}

	if (stat(full_filename, &fileinfo) == -1) {
		/* Well, maybe we can open the file even if the OS says its not there */
		if ((fd = open(filename.c_str(), O_RDONLY)) != -1) {
			if (!quiet) {
				statusbar(_("Reading File"));
			}
			return 0;
		}

		if (newfie) {
			if (!quiet) {
				statusbar(_("New File"));
			}
			return -2;
		}
		statusbar(_("\"%s\" not found"), filename.c_str());
		beep();
		return -1;
	} else if (S_ISDIR(fileinfo.st_mode) || S_ISCHR(fileinfo.st_mode) || S_ISBLK(fileinfo.st_mode)) {
		/* Don't open directories, character files, or block files. Sorry, /dev/sndstat! */
		statusbar(S_ISDIR(fileinfo.st_mode) ? _("\"%s\" is a directory") : _("\"%s\" is a device file"), filename.c_str());
		beep();
		return -1;
	} else if ((fd = open(full_filename.c_str(), O_RDONLY)) == -1) {
		statusbar(_("Error reading %s: %s"), filename.c_str(), strerror(errno));
		beep();
		return -1;
	} else {
		/* The file is A-OK.  Open it. */
		*f = fdopen(fd, "rb");

		if (*f == NULL) {
			statusbar(_("Error reading %s: %s"), filename.c_str(), strerror(errno));
			beep();
			close(fd);
		} else {
			statusbar(_("Reading File"));
		}
	}

	return fd;
}

/* This function will return the name of the first available extension
 * of a filename (starting with [name][suffix], then [name][suffix].1,
 * etc.).  Memory is allocated for the return value.  If no writable
 * extension exists, we return "". */
std::string get_next_filename(const std::string& name, const std::string& suffix)
{
	static int ulmax_digits = -1;
	unsigned long i = 0;

	if (ulmax_digits == -1) {
		ulmax_digits = digits(ULONG_MAX);
	}

	std::string buf = name + suffix;

	while (true) {
		struct stat fs;

		if (stat(buf, &fs) == -1) {
			return buf;
		}
		if (i == ULONG_MAX) {
			break;
		}

		buf = name + suffix + "." + std::to_string(++i);
	}

	/* We get here only if there is no possible save file. */
	return "";
}

/* Execute a command without inserting any output. The statusbar will show
 * whether the command succeeded (exit code of 0) or failed (with the
 * process's exit code shown). */
void do_execute_command()
{
	int status;
	std::shared_ptr<Key> key;
	char statusbartext[50];
	const char *msg;
	std::string ans;
	currmenu = MEXTCMD;

	while (true) {
		msg = _("Command to execute (no output) [from %s]");

		PromptResult i = do_prompt(true, true, MEXTCMD, key, ans, NULL, edit_refresh, msg, "./");

		/* Cancel if the command was empty or the user cancelled */
		if (i == PROMPT_ABORTED || (i == PROMPT_BLANK_STRING || answer.front() == '\n')) {
			statusbar(_("Cancelled"));
			break;
		} else {
			ans = answer;

			auto func = func_from_key(*key);

			if (func == to_files_void) {
				std::string tmp = do_browse_from(answer);

				if (tmp == "") {
					continue;
				}

				/* User selected a file */
				answer = tmp;
			} else {
				/* If we don't have a file yet, go back to the prompt. */
				continue;
			}

			/* Convert newlines to nulls before executing the command. */
			sunder(answer);

			/* Execute the command without savings its output. */
			status = execute_command_silently(answer);

			/* Update the screen. */
			edit_refresh();

			/* Show success/failure indication in the status bar. */
			if (status == 0) {
				sprintf(statusbartext, _("Command completed successfully"));
			} else if (status == COMMAND_FAILED_PERMISSION_DENIED) {
				sprintf(statusbartext, _("Execution failed: permission denied"));
			} else if (status == COMMAND_FAILED_NOT_FOUND) {
				sprintf(statusbartext, _("Execution failed: command not found"));
			} else {
				sprintf(statusbartext, _("Command failed with code %d"), status);
			}

			statusbar(statusbartext);

			break;
		}
	}
	shortcut_init();

	display_main_list();
}

/* Insert a file into a new buffer if the MULTIBUFFER flag is set, or
 * into the current buffer if it isn't.  If execute is true, insert the
 * output of an executed command instead of a file. */
void do_insertfile(bool execute)
{
	const char *msg;
	std::string ans;
	/* The last answer the user typed at the statusbar prompt. */
	filestruct *edittop_save = openfile->edittop;
	size_t current_x_save = openfile->current_x;
	ssize_t current_y_save = openfile->current_y;
	bool edittop_inside = false;
	bool right_side_up = false, single_line = false;

	currmenu = MINSERTFILE;

	while (true) {
		if (execute) {
			msg = ISSET(MULTIBUFFER) ? _("Command to execute in new buffer [from %s] ") : _("Command to execute [from %s] ");
		} else {
			msg = ISSET(MULTIBUFFER) ? _("File to insert into new buffer [from %s] ") : _("File to insert [from %s] ");
		}

		std::shared_ptr<Key> key;
		PromptResult i = do_prompt(true, true, execute ? MEXTCMD : MINSERTFILE, key, ans, NULL, edit_refresh, msg, "./");

		/* If we're in multibuffer mode and the filename or command is
		 * blank, open a new buffer instead of canceling.  If the
		 * filename or command begins with a newline (i.e. an encoded
		 * null), treat it as though it's blank. */
		if (i == PROMPT_ABORTED || ((i == PROMPT_BLANK_STRING || answer.front() == '\n') && !ISSET(MULTIBUFFER))) {
			statusbar(_("Cancelled"));
			break;
		} else {
			size_t pww_save = openfile->placewewant;

			ans = answer;

			auto func = func_from_key(*key);

			if (func == new_buffer_void) {
				/* Don't allow toggling if we're in view mode. */
				if (!ISSET(VIEW_MODE)) {
					TOGGLE(MULTIBUFFER);
				}
				continue;
			} else {
				if (func == toggle_execute_void) {
					execute = !execute;
					continue;
				} else if (func == to_files_void) {
					std::string tmp = do_browse_from(answer);

					if (tmp == "") {
						continue;
					}

					/* We have a file now.  Indicate this. */
					answer = tmp;

					i = PROMPT_ENTER_PRESSED;
				}
			}

			/* If we don't have a file yet, go back to the statusbar prompt. */
			if (i != PROMPT_ENTER_PRESSED && (i != PROMPT_BLANK_STRING || !ISSET(MULTIBUFFER))) {
				continue;
			}

			/* Keep track of whether the mark begins inside the
			 * partition and will need adjustment. */
			if (openfile->mark_set) {
				filestruct *top = nullptr, *bot = nullptr;
				size_t top_x, bot_x;

				mark_order((const filestruct **)&top, &top_x, (const filestruct **)&bot, &bot_x, &right_side_up);

				single_line = (top == bot);
			}

			if (!ISSET(MULTIBUFFER)) {
				/* If we're not inserting into a new buffer, partition
				 * the filestruct so that it contains no text and hence
				 * looks like a new buffer, and keep track of whether
				 * the top of the edit window is inside the
				 * partition. */
				filepart = partition_filestruct(openfile->current, openfile->current_x, openfile->current, openfile->current_x);
				edittop_inside = (openfile->edittop == openfile->fileage);
			}

			/* Convert newlines to nulls, just before we insert the file
			 * or execute the command. */
			sunder(answer);

			if (execute) {
				if (ISSET(MULTIBUFFER)) {
					/* Open a blank buffer. */
					open_buffer("", false);
				}

				/* Save the command's output in the current buffer. */
				execute_command(answer);

				if (ISSET(MULTIBUFFER)) {
					/* Move back to the beginning of the first line of
					 * the buffer. */
					openfile->current = openfile->fileage;
					openfile->current_x = 0;
					openfile->placewewant = 0;
				}
			} else {
				/* Make sure the path to the file specified in answer is tilde-expanded. */
				answer = real_dir_from_tilde(answer);

				/* Save the file specified in answer in the current buffer. */
				open_buffer(answer, true);
			}

			if (ISSET(MULTIBUFFER)) {
				/* Update the screen to account for the current buffer. */

				ssize_t savedposline, savedposcol;

				display_buffer();

				if (!execute && ISSET(POS_HISTORY) && check_poshistory(answer, &savedposline, &savedposcol)) {
					do_gotolinecolumn(savedposline, savedposcol, false, false, false, false);
				}
			} else {
				filestruct *top_save = openfile->fileage;

				/* If we were at the top of the edit window before, set
				 * the saved value of edittop to the new top of the edit
				 * window. */
				if (edittop_inside) {
					edittop_save = openfile->fileage;
				}

				/* Update the current x-coordinate to account for the
				 * number of characters inserted on the current line.
				 * If the mark begins inside the partition, adjust the
				 * mark coordinates to compensate for the change in the
				 * current line. */
				openfile->current_x = strlen(openfile->filebot->data);
				if (openfile->fileage == openfile->filebot) {
					if (openfile->mark_set) {
						openfile->mark_begin = openfile->current;
						if (!right_side_up)
							openfile->mark_begin_x += openfile->current_x;
					}
					openfile->current_x += current_x_save;
				} else if (openfile->mark_set) {
					if (!right_side_up) {
						if (single_line) {
							openfile->mark_begin = openfile->current;
							openfile->mark_begin_x -= current_x_save;
						} else
							openfile->mark_begin_x -= openfile->current_x;
					}
				}

				/* Update the current y-coordinate to account for the
				 * number of lines inserted. */
				openfile->current_y += current_y_save;

				/* Unpartition the filestruct so that it contains all
				 * the text again.  Note that we've replaced the
				 * non-text originally in the partition with the text in
				 * the inserted file/executed command output. */
				unpartition_filestruct(&filepart);

				/* Renumber starting with the beginning line of the old partition. */
				renumber(top_save);

				/* Restore the old edittop. */
				openfile->edittop = edittop_save;

				/* Restore the old place we want. */
				openfile->placewewant = pww_save;

				/* Mark the file as modified. */
				set_modified();

				/* Update the screen. */
				edit_refresh();
			}

			break;
		}
	}
	shortcut_init();
}

/* Insert a file into a new buffer or the current buffer, depending on
 * whether the MULTIBUFFER flag is set.  If we're in view mode, only
 * allow inserting a file into a new buffer. */
void do_insertfile_void(void)
{
	if (ISSET(VIEW_MODE) && !ISSET(MULTIBUFFER)) {
		statusbar(_("Key invalid in non-multibuffer mode"));
	} else {
		do_insertfile(false);
	}

	display_main_list();
}

/* When passed "[relative path]" or "[relative path][filename]" in
 * origpath, return "[full path]" or "[full path][filename]" on success,
 * or NULL on error.  Do this if the file doesn't exist but the relative
 * path does, since the file could exist in memory but not yet on disk).
 * Don't do this if the relative path doesn't exist, since we won't be
 * able to go there. */
std::string get_full_path(const std::string& origpath)
{
	if (origpath == "") {
		return "";
	}

	struct stat fileinfo;
	bool path_only;
	std::string d_there_file;

	/* Get the current directory.  If it doesn't exist, back up and try
	 * again until we get a directory that does, and use that as the
	 * current directory. */
	auto d_here = getcwd();

	while (d_here == "") {
		if (chdir("..") == -1) {
			break;
		}

		d_here = getcwd();
	}

	/* If we succeeded, canonicalize it in d_here. */
	if (d_here != "") {
		/* If the current directory isn't "/", tack a slash onto the end of it. */
		if (d_here != "/") {
			d_here += "/";
		}
	}

	auto d_there = real_dir_from_tilde(origpath);

	/* If stat()ing d_there fails, assume that d_there refers to a new
	 * file that hasn't been saved to disk yet.  Set path_only to true
	 * if d_there refers to a directory, and false otherwise. */
	path_only = (stat(d_there, &fileinfo) != -1 && S_ISDIR(fileinfo.st_mode));

	/* If path_only is true, make sure d_there ends in a slash. */
	if (path_only) {
		if (d_there[d_there.length() - 1] != '/') {
			d_there += "/";
		}
	}

	/* Search for the last slash in d_there. */
	auto last_slash = d_there.rfind('/');

	/* If we didn't find one, then make sure the answer is in the format "d_here/d_there". */
	if (last_slash == std::string::npos) {
		assert(!path_only);

		d_there_file = d_there;
		d_there = d_here;
	} else {
		/* If path_only is false, then save the filename portion of the
		 * answer (everything after the last slash) in d_there_file. */
		if (!path_only) {
			d_there_file = d_there.substr(last_slash + 1);
		}

		/* Remove the filename portion of the answer from d_there. */
		d_there = d_there.substr(0, last_slash);

		/* Go to the path specified in d_there. */
		if (chdir(d_there) == -1) {
			d_there = "";
		} else {
			/* Get the full path. */
			d_there = getcwd();

			/* If we succeeded, canonicalize it in d_there. */
			if (d_there != "") {
				/* If the current directory isn't "/", tack a slash onto the end of it. */
				if (d_there != "/") {
					d_there += "/";
				}
			} else {
				/* Otherwise, set path_only to true, so that we clean up
				 * correctly, free all allocated memory, and return "". */
				path_only = true;
			}

			/* Finally, go back to the path specified in d_here,
			 * where we were before.  We don't check for a chdir()
			 * error, since we can do nothing if we get one. */
			IGNORE_CALL_RESULT(chdir(d_here));
		}
	}

	/* At this point, if path_only is false and d_there isn't "",
	 * d_there contains the path portion of the answer and d_there_file
	 * contains the filename portion of the answer.  If this is the
	 * case, tack the latter onto the end of the former.  d_there will
	 * then contain the complete answer. */
	if (!path_only && d_there != "") {
		d_there += d_there_file;
	}

	return d_there;
}

/* Return the full version of path, as returned by get_full_path().  On
 * error, if path doesn't reference a directory, or if the directory
 * isn't writable, return "". */
std::string check_writable_directory(const std::string& path)
{
	std::string full_path = get_full_path(path);

	/* If get_full_path() fails, return "". */
	if (full_path == "") {
		return "";
	}

	/* If we can't write to path or path isn't a directory, return "". */
	if (access(full_path, W_OK) != 0 || full_path.back() != '/') {
		return "";
	}

	/* Otherwise, return the full path. */
	return full_path;
}

/* This function calls mkstemp(($TMPDIR|P_tmpdir|/tmp/)"pinot.XXXXXX").
 * On success, it returns the malloc()ed filename and corresponding FILE
 * stream, opened in "r+b" mode.  On error, it returns "" for the
 * filename and leaves the FILE stream unchanged. */
std::string safe_tempfile(FILE **f)
{
	std::string full_tempdir;
	const char *tmpdir_env;
	int fd;
	mode_t original_umask = 0;

	assert(f != NULL);

	/* If $TMPDIR is set, set tempdir to it, run it through
	 * get_full_path(), and save the result in full_tempdir.  Otherwise,
	 * leave full_tempdir set to NULL. */
	tmpdir_env = getenv("TMPDIR");
	if (tmpdir_env != NULL) {
		full_tempdir = check_writable_directory(tmpdir_env);
	}

	/* If $TMPDIR is unset, empty, or not a writable directory, and
	 * full_tempdir is "", try P_tmpdir instead. */
	if (full_tempdir == "") {
		full_tempdir = check_writable_directory(P_tmpdir);
	}

	/* if P_tmpdir is "", use /tmp. */
	if (full_tempdir == "") {
		full_tempdir = "/tmp/";
	}

	full_tempdir += "pinot.XXXXXX";

	original_umask = umask(0);
	umask(S_IRWXG | S_IRWXO);

	fd = mkstemp(full_tempdir);

	if (fd != -1) {
		*f = fdopen(fd, "r+b");
	} else {
		full_tempdir = "";
	}

	umask(original_umask);

	return full_tempdir;
}

/* Although this sucks, it sucks less than having a single 'my system is messed up
 * and I'm blanket allowing insecure file writing operations.
 */

int prompt_failed_backupwrite(const std::string& filename)
{
	static int i;
	static std::string prevfile;; /* What was the last file we were paased so we don't keep asking this? though maybe we should.... */
	if (prevfile == "" || filename != prevfile) {
		i = do_yesno_prompt(false, _("Failed to write backup file, continue saving? (Say N if unsure) "));
		prevfile = filename;
	}
	return i;
}

void init_backup_dir(void)
{
	if (backup_dir == "") {
		return;
	}

	auto full_backup_dir = get_full_path(backup_dir);

	/* If get_full_path() failed or the backup directory is
	 * inaccessible, unset backup_dir. */
	if (full_backup_dir == "" || full_backup_dir[full_backup_dir.length() - 1] != '/') {
		backup_dir = "";
	} else {
		backup_dir = full_backup_dir;
	}
}

/* Read from inn, write to out.  We assume inn is opened for reading,
 * and out for writing.  We return 0 on success, -1 on read error, or -2
 * on write error. */
int copy_file(FILE *inn, FILE *out)
{
	int retval = 0;
	char buf[BUFSIZ];
	size_t charsread;

	assert(inn != NULL && out != NULL && inn != out);

	do {
		charsread = fread(buf, sizeof(char), BUFSIZ, inn);
		if (charsread == 0 && ferror(inn)) {
			retval = -1;
			break;
		}
		if (fwrite(buf, sizeof(char), charsread, out) < charsread) {
			retval = -2;
			break;
		}
	} while (charsread > 0);

	if (fclose(inn) == EOF) {
		retval = -1;
	}
	if (fclose(out) == EOF) {
		retval = -2;
	}

	return retval;
}

/* Write a file out to disk.  If f_open isn't NULL, we assume that it is
 * a stream associated with the file, and we don't try to open it
 * ourselves.  If tmp is true, we set the umask to disallow anyone else
 * from accessing the file, we don't set the filename to its name, and
 * we don't print out how many lines we wrote on the statusbar.
 *
 * tmp means we are writing a temporary file in a secure fashion.  We
 * use it when spell checking or dumping the file on an error.  If
 * append is APPEND, it means we are appending instead of overwriting.
 * If append is PREPEND, it means we are prepending instead of
 * overwriting.  If nonamechange is true, we don't change the current
 * filename.  nonamechange is ignored if tmp is false, we're appending,
 * or we're prepending.
 *
 * Return true on success or false on error. */
bool write_file(const std::string& name, FILE *f_open, bool tmp, AppendType append, bool nonamechange)
{
	bool retval = false;
	/* Instead of returning in this function, you should always
	 * set retval and then goto cleanup_and_exit. */
	size_t lineswritten = 0;
	const filestruct *fileptr = openfile->fileage;
	int fd;
	/* The file descriptor we use. */
	mode_t original_umask = 0;
	/* Our umask, from when pinot started. */
	bool realexists;
	/* The result of stat().  true if the file exists, false
	 * otherwise.  If name is a link that points nowhere, realexists
	 * is false. */
	struct stat st;
	/* The status fields filled in by stat(). */
	bool anyexists;
	/* The result of lstat().  The same as realexists, unless name
	 * is a link. */
	struct stat lst;
	/* The status fields filled in by lstat(). */
	std::string realname;
	/* name after tilde expansion. */
	FILE *f = NULL;
	/* The actual file, realname, we are writing to. */
	std::string tempname;
	/* The temp file name we write to on prepend. */

	if (name == "") {
		return -1;
	}

	if (f_open != NULL) {
		f = f_open;
	}

	if (!tmp) {
		titlebar(NULL);
	}

	realname = real_dir_from_tilde(name);

	anyexists = (lstat(realname, &lst) != -1);

	/* If the temp file exists and isn't already open, give up. */
	if (tmp && anyexists && f_open == NULL) {
		goto cleanup_and_exit;
	}

	/* If NOFOLLOW_SYMLINKS is set, it doesn't make sense to prepend or
	 * append to a symlink.  Here we warn about the contradiction. */
	if (ISSET(NOFOLLOW_SYMLINKS) && anyexists && S_ISLNK(lst.st_mode)) {
		statusbar(_("Cannot prepend or append to a symlink with --nofollow set"));
		goto cleanup_and_exit;
	}

	/* Save the state of the file at the end of the symlink (if there is
	 * one). */
	realexists = (stat(realname, &st) != -1);

	/* if we have not stat()d this file before (say, the user just
	 * specified it interactively), stat and save the value
	 * or else we will chase null pointers when we do
	 * modtime checks, preserve file times, etc. during backup */
	if (openfile->current_stat == nullptr && !tmp && realexists) {
		openfile->current_stat = new struct stat;
		stat(realname, openfile->current_stat);
	}

	/* We backup only if the backup toggle is set, the file isn't
	 * temporary, and the file already exists.  Furthermore, if we
	 * aren't appending, prepending, or writing a selection, we backup
	 * only if the file has not been modified by someone else since pinot
	 * opened it. */
	if (ISSET(BACKUP_FILE) && !tmp && realexists && ((append != OVERWRITE || openfile->mark_set) || (openfile->current_stat && openfile->current_stat->st_mtime == st.st_mtime))) {
		int backup_fd;
		FILE *backup_file;
		std::string backupname;
		struct utimbuf filetime;
		int copy_status;
		int backup_cflags;

		/* Save the original file's access and modification times. */
		filetime.actime = openfile->current_stat->st_atime;
		filetime.modtime = openfile->current_stat->st_mtime;

		if (f_open == NULL) {
			/* Open the original file to copy to the backup. */
			f = fopen(realname, "rb");

			if (f == NULL) {
				statusbar(_("Error reading %s: %s"), realname.c_str(), strerror(errno));
				beep();
				/* If we can't read from the original file, go on, since
				 * only saving the original file is better than saving
				 * nothing. */
				goto skip_backup;
			}
		}

		/* If backup_dir is set, we set backupname to
		 * backup_dir/backupname~[.number], where backupname is the
		 * canonicalized absolute pathname of realname with every '/'
		 * replaced with a '!'.  This means that /home/foo/file is
		 * backed up in backup_dir/!home!foo!file~[.number]. */
		if (backup_dir != "") {
			auto backuptemp = get_full_path(realname);

			if (backuptemp == "") {
				/* If get_full_path() failed, we don't have a
				 * canonicalized absolute pathname, so just use the
				 * filename portion of the pathname.  We use tail() so
				 * that e.g. ../backupname will be backed up in
				 * backupdir/backupname~ instead of
				 * backupdir/../backupname~. */
				backuptemp = tail(realname);
			} else {
				for (size_t i = 0; backuptemp[i] != '\0'; i++) {
					if (backuptemp[i] == '/') {
						backuptemp[i] = '!';
					}
				}
			}

			backupname = backup_dir + backuptemp;
			backuptemp = get_next_filename(backupname, "~");
			if (backuptemp == "") {
				statusbar(_("Error writing backup file %s: %s"), backupname.c_str(), _("Too many backup files?"));
				/* If we can't write to the backup, DONT go on, since
				   whatever caused the backup file to fail (e.g. disk
				   full may well cause the real file write to fail, which
				   means we could lose both the backup and the original! */
				goto cleanup_and_exit;
			} else {
				backupname = backuptemp;
			}
		} else {
			backupname = realname + "~";
		}

		/* First, unlink any existing backups.  Next, open the backup
		   file with O_CREAT and O_EXCL.  If it succeeds, we
		   have a file descriptor to a new backup file. */
		if (unlink(backupname) < 0 && errno != ENOENT && !ISSET(INSECURE_BACKUP)) {
			if (prompt_failed_backupwrite(backupname)) {
				goto skip_backup;
			}
			statusbar(_("Error writing backup file %s: %s"), backupname.c_str(), strerror(errno));
			goto cleanup_and_exit;
		}

		if (ISSET(INSECURE_BACKUP)) {
			backup_cflags = O_WRONLY | O_CREAT | O_APPEND;
		} else {
			backup_cflags = O_WRONLY | O_CREAT | O_EXCL | O_APPEND;
		}

		backup_fd = open(backupname.c_str(), backup_cflags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		/* Now we've got a safe file stream.  If the previous open()
		   call failed, this will return NULL. */
		backup_file = fdopen(backup_fd, "wb");

		if (backup_fd < 0 || backup_file == NULL) {
			statusbar(_("Error writing backup file %s: %s"), backupname.c_str(), strerror(errno));
			goto cleanup_and_exit;
		}

		/* We shouldn't worry about chown()ing something if we're not
		root, since it's likely to fail! */
		if (geteuid() == PINOT_ROOT_UID && fchown(backup_fd, openfile->current_stat->st_uid, openfile->current_stat->st_gid) == -1 && !ISSET(INSECURE_BACKUP)) {
			if (prompt_failed_backupwrite(backupname)) {
				goto skip_backup;
			}
			statusbar(_("Error writing backup file %s: %s"), backupname.c_str(), strerror(errno));
			fclose(backup_file);
			goto cleanup_and_exit;
		}

		if (fchmod(backup_fd, openfile->current_stat->st_mode) == -1 && !ISSET(INSECURE_BACKUP)) {
			if (prompt_failed_backupwrite(backupname)) {
				goto skip_backup;
			}
			statusbar(_("Error writing backup file %s: %s"), backupname.c_str(), strerror(errno));
			fclose(backup_file);
			/* If we can't write to the backup, DONT go on, since
			   whatever caused the backup file to fail (e.g. disk
			   full may well cause the real file write to fail, which
			   means we could lose both the backup and the original! */
			goto cleanup_and_exit;
		}

		DEBUG_LOG("Backing up " << realname << " to " << backupname);

		/* Copy the file. */
		copy_status = copy_file(f, backup_file);

		if (copy_status != 0) {
			statusbar(_("Error reading %s: %s"), realname.c_str(), strerror(errno));
			beep();
			goto cleanup_and_exit;
		}

		/* And set its metadata. */
		if (utime(backupname.c_str(), &filetime) == -1 && !ISSET(INSECURE_BACKUP)) {
			if (prompt_failed_backupwrite(backupname)) {
				goto skip_backup;
			}
			statusbar(_("Error writing backup file %s: %s"), backupname.c_str(), strerror(errno));
			/* If we can't write to the backup, DONT go on, since
			   whatever caused the backup file to fail (e.g. disk
			   full may well cause the real file write to fail, which
			   means we could lose both the backup and the original! */
			goto cleanup_and_exit;
		}
	}

skip_backup:

	/* If NOFOLLOW_SYMLINKS is set and the file is a link, we aren't
	 * doing prepend or append.  So we delete the link first, and just
	 * overwrite. */
	if (ISSET(NOFOLLOW_SYMLINKS) && anyexists && S_ISLNK(lst.st_mode) && unlink(realname) == -1) {
		statusbar(_("Error writing %s: %s"), realname.c_str(), strerror(errno));
		goto cleanup_and_exit;
	}

	if (f_open == NULL) {
		original_umask = umask(0);

		/* If we create a temp file, we don't let anyone else access it.
		 * We create a temp file if tmp is true. */
		if (tmp) {
			umask(S_IRWXG | S_IRWXO);
		} else {
			umask(original_umask);
		}
	}

	/* If we're prepending, copy the file to a temp file. */
	if (append == PREPEND) {
		int fd_source;
		FILE *f_source = NULL;

		if (f == NULL) {
			f = fopen(realname, "rb");

			if (f == NULL) {
				statusbar(_("Error reading %s: %s"), realname.c_str(), strerror(errno));
				beep();
				goto cleanup_and_exit;
			}
		}

		tempname = safe_tempfile(&f);

		if (tempname == "") {
			statusbar(_("Error writing temp file: %s"), strerror(errno));
			goto cleanup_and_exit;
		}

		if (f_open == NULL) {
			fd_source = open(realname.c_str(), O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR);

			if (fd_source != -1) {
				f_source = fdopen(fd_source, "rb");
				if (f_source == NULL) {
					statusbar(_("Error reading %s: %s"), realname.c_str(), strerror(errno));
					beep();
					close(fd_source);
					fclose(f);
					unlink(tempname);
					goto cleanup_and_exit;
				}
			}
		}

		if (copy_file(f_source, f) != 0) {
			statusbar(_("Error writing %s: %s"), tempname.c_str(), strerror(errno));
			unlink(tempname);
			goto cleanup_and_exit;
		}
	}

	if (f_open == NULL) {
		/* Now open the file in place.  Use O_EXCL if tmp is true.  This
		 * is copied from joe, because wiggy says so *shrug*. */
		fd = open(realname.c_str(), O_WRONLY | O_CREAT | ((append == APPEND) ? O_APPEND : (tmp ? O_EXCL : O_TRUNC)), S_IRUSR |S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

		/* Set the umask back to the user's original value. */
		umask(original_umask);

		/* If we couldn't open the file, give up. */
		if (fd == -1) {
			statusbar(_("Error writing %s: %s"), realname.c_str(), strerror(errno));

			/* tempname has been set only if we're prepending. */
			if (tempname != "") {
				unlink(tempname);
			}
			goto cleanup_and_exit;
		}

		f = fdopen(fd, (append == APPEND) ? "ab" : "wb");

		if (f == NULL) {
			statusbar(_("Error writing %s: %s"), realname.c_str(), strerror(errno));
			close(fd);
			goto cleanup_and_exit;
		}
	}

	/* There might not be a magicline.  There won't be when writing out a selection. */
	assert(openfile->fileage != NULL && openfile->filebot != NULL);

	while (fileptr != NULL) {
		size_t data_len = strlen(fileptr->data), size;

		/* Convert newlines to nulls, just before we write to disk. */
		sunder(fileptr->data);

		size = fwrite(fileptr->data, sizeof(char), data_len, f);

		/* Convert nulls to newlines.  data_len is the string's real length. */
		unsunder(fileptr->data, data_len);

		if (size < data_len) {
			statusbar(_("Error writing %s: %s"), realname.c_str(), strerror(errno));
			fclose(f);
			goto cleanup_and_exit;
		}

		/* If we're on the last line of the file, don't write a newline
		 * character after it.  If the last line of the file is blank,
		 * this means that zero bytes are written, in which case we
		 * don't count the last line in the total lines written. */
		if (fileptr == openfile->filebot) {
			if (fileptr->data[0] == '\0') {
				lineswritten--;
			}
		} else {
			if (openfile->fmt == DOS_FILE || openfile->fmt == MAC_FILE) {
				if (putc('\r', f) == EOF) {
					statusbar(_("Error writing %s: %s"), realname.c_str(), strerror(errno));
					fclose(f);
					goto cleanup_and_exit;
				}
			}

			if (openfile->fmt != MAC_FILE) {
				if (putc('\n', f) == EOF) {
					statusbar(_("Error writing %s: %s"), realname.c_str(), strerror(errno));
					fclose(f);
					goto cleanup_and_exit;
				}
			}
		}

		fileptr = fileptr->next;
		lineswritten++;
	}

	/* If we're prepending, open the temp file, and append it to f. */
	if (append == PREPEND) {
		int fd_source;
		FILE *f_source = NULL;

		fd_source = open(tempname.c_str(), O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR);

		if (fd_source != -1) {
			f_source = fdopen(fd_source, "rb");
			if (f_source == NULL) {
				close(fd_source);
			}
		}

		if (f_source == NULL) {
			statusbar(_("Error reading %s: %s"), tempname.c_str(), strerror(errno));
			beep();
			fclose(f);
			goto cleanup_and_exit;
		}

		if (copy_file(f_source, f) == -1 || unlink(tempname) == -1) {
			statusbar(_("Error writing %s: %s"), realname.c_str(), strerror(errno));
			goto cleanup_and_exit;
		}
	} else if (fclose(f) != 0) {
		statusbar(_("Error writing %s: %s"), realname.c_str(), strerror(errno));
		goto cleanup_and_exit;
	}

	if (!tmp && append == OVERWRITE) {
		if (!nonamechange) {
			openfile->filename = realname;
			/* We might have changed the filename, so update the colors
			 * to account for it, and then make sure we're using them. */
			color_update();
			color_init();

			/* If color syntaxes are available and turned on, we need to
			 * call edit_refresh(). */
			if (!openfile->colorstrings.empty() && !ISSET(NO_COLOR_SYNTAX)) {
				edit_refresh();
			}
		}

		/* Update current_stat to reference the file as it is now. */
		if (openfile->current_stat == nullptr) {
			openfile->current_stat = new struct stat;
		}
		if (!openfile->mark_set) {
			stat(realname, openfile->current_stat);
		}

		statusbar(P_("Wrote %lu line", "Wrote %lu lines", (unsigned long)lineswritten), (unsigned long)lineswritten);
		openfile->modified = false;
		titlebar(NULL);
	}

	retval = true;

cleanup_and_exit:
	return retval;
}

/* Write a marked selection from a file out to disk.  Return true on
 * success or false on error. */
bool write_marked_file(const std::string& name, FILE *f_open, bool tmp, AppendType append)
{
	bool retval;
	bool old_modified = openfile->modified;
	/* write_file() unsets the modified flag. */
	bool added_magicline = false;
	/* Whether we added a magicline after filebot. */
	filestruct *top = nullptr, *bot = nullptr;
	size_t top_x, bot_x;

	assert(openfile->mark_set);

	/* Partition the filestruct so that it contains only the marked text. */
	mark_order((const filestruct **)&top, &top_x, (const filestruct **)&bot, &bot_x, NULL);
	filepart = partition_filestruct(top, top_x, bot, bot_x);

	/* Handle the magicline if the NO_NEWLINES flag isn't set.  If the
	 * line at filebot is blank, treat it as the magicline and hence the
	 * end of the file.  Otherwise, add a magicline and treat it as the
	 * end of the file. */
	if (!ISSET(NO_NEWLINES) && (added_magicline = (openfile->filebot->data[0] != '\0'))) {
		new_magicline();
	}

	retval = write_file(name, f_open, tmp, append, true);

	/* If the NO_NEWLINES flag isn't set, and we added a magicline, remove it now. */
	if (!ISSET(NO_NEWLINES) && added_magicline) {
		remove_magicline();
	}

	/* Unpartition the filestruct so that it contains all the text again. */
	unpartition_filestruct(&filepart);

	if (old_modified) {
		set_modified();
	}

	return retval;
}

/* Write the current file to disk.  If the mark is on, write the current
 * marked selection to disk.  If exiting is true, write the file to disk
 * regardless of whether the mark is on, and without prompting if the
 * TEMP_FILE flag is set.  Return true on success or false on error. */
bool do_writeout(bool exiting)
{
	AppendType append = OVERWRITE;
	std::string ans;
	/* The last answer the user typed at the statusbar prompt. */
	bool retval = false;

	currmenu = MWRITEFILE;

	if (exiting && openfile->filename != "" && ISSET(TEMP_FILE)) {
		retval = write_file(openfile->filename, NULL, false, OVERWRITE, false);

		/* Write succeeded. */
		if (retval) {
			return retval;
		}
	}

	ans = (!exiting && openfile->mark_set) ? "" : openfile->filename;

	while (true) {
		const char *msg;
		const char *formatstr, *backupstr;

		formatstr = (openfile->fmt == DOS_FILE) ? _(" [DOS Format]") : (openfile->fmt == MAC_FILE) ? _(" [Mac Format]") : "";

		backupstr = ISSET(BACKUP_FILE) ? _(" [Backup]") : "";

		if (!exiting && openfile->mark_set) {
			msg = (append == PREPEND) ?
			      _("Prepend Selection to File") : (append == APPEND) ?
			      _("Append Selection to File") :
			      _("Write Selection to File");
		} else {
			msg = (append == PREPEND) ? _("File Name to Prepend to") :
			      (append == APPEND) ? _("File Name to Append to") :
			      _("File Name to Write");
		}

		std::shared_ptr<Key> key;
		PromptResult i = do_prompt(true,
		              true,
		              MWRITEFILE, key, ans,
		              NULL,
		              edit_refresh, "%s%s%s", msg,
		              formatstr, backupstr
		             );

		/* If the filename or command begins with a newline (i.e. an
		 * encoded null), treat it as though it's blank. */
		if (i == PROMPT_ABORTED || i == PROMPT_BLANK_STRING || answer.front() == '\n') {
			statusbar(_("Cancelled"));
			retval = false;
			break;
		} else {
			ans = answer;
			auto func = func_from_key(*key);

			if (func == to_files_void) {
				std::string tmp = do_browse_from(answer);

				if (tmp == "") {
					continue;
				}

				/* We have a file now.  Indicate this. */
				answer = tmp;
			} else {
				if (func == dos_format_void) {
					openfile->fmt = (openfile->fmt == DOS_FILE) ? NIX_FILE : DOS_FILE;
					continue;
				} else if (func == mac_format_void) {
					openfile->fmt = (openfile->fmt == MAC_FILE) ? NIX_FILE : MAC_FILE;
					continue;
				} else if (func == backup_file_void) {
					TOGGLE(BACKUP_FILE);
					continue;
				} else if (func == prepend_void) {
					append = (append == PREPEND) ? OVERWRITE : PREPEND;
					continue;
				} else if (func == append_void) {
					append = (append == APPEND) ? OVERWRITE : APPEND;
					continue;
				} else if (func == do_help_void) {
					continue;
				}
			}

			DEBUG_LOG("filename is " << answer);

			if (append == OVERWRITE) {
				bool name_exists, do_warning;
				std::string full_answer;
				struct stat st;

				/* Convert newlines to nulls, just before we get the full path. */
				sunder(answer);

				full_answer = get_full_path(answer);
				std::string full_filename = get_full_path(openfile->filename);
				name_exists = (stat((full_answer == "") ? answer : full_answer, &st) != -1);
				if (openfile->filename.front() == '\0') {
					do_warning = name_exists;
				} else {
					do_warning = ((full_answer == "") ? answer : full_answer) != ((full_filename == "") ? openfile->filename : full_filename);
				}

				/* Convert nulls to newlines. */
				unsunder(answer);

				if (do_warning) {
					if (name_exists) {
						YesNoPromptResult prompt_result = do_yesno_prompt(false, _("File exists, OVERWRITE ? "));
						if (prompt_result == YESNO_PROMPT_NO || prompt_result == YESNO_PROMPT_ABORTED) {
							continue;
						}
					} else if (exiting || !openfile->mark_set) {
						YesNoPromptResult prompt_result = do_yesno_prompt(false, _("Save file under DIFFERENT NAME ? "));
						if (prompt_result == YESNO_PROMPT_NO || prompt_result == YESNO_PROMPT_ABORTED) {
							continue;
						}
					}
				}
				/* Complain if the file exists, the name hasn't changed, and the
				    stat information we had before does not match what we have now */
				else if (name_exists && openfile->current_stat && (openfile->current_stat->st_mtime < st.st_mtime ||
				         openfile->current_stat->st_dev != st.st_dev || openfile->current_stat->st_ino != st.st_ino)) {
					YesNoPromptResult prompt_result = do_yesno_prompt(false, _("File was modified since you opened it, continue saving ? "));
					if (prompt_result == YESNO_PROMPT_NO || prompt_result == YESNO_PROMPT_ABORTED) {
						continue;
					}
				}

			}

			/* Convert newlines to nulls, just before we save the file. */
			sunder(answer);

			/* Here's where we allow the selected text to be written to a separate file. */
			retval =
			    (!exiting && openfile->mark_set) ?
			    write_marked_file(answer, NULL, false, append) :
			    write_file(answer, NULL, false, append, false);

			break;
		}
	}

	return retval;
}

/* Write the current file to disk.  If the mark is on, write the current
 * marked selection to disk. */
void do_writeout_void(void)
{
	do_writeout(false);
	display_main_list();
}

/* Return a malloc()ed string containing the actual directory, used to
 * convert ~user/ and ~/ notation. */
std::string real_dir_from_tilde(const std::string& buf)
{
	char *dir = real_dir_from_tilde(buf.c_str());
	std::string result(dir);
	free(dir);

	return result;
}

char *real_dir_from_tilde(const char *buf)
{
	char *retval;

	assert(buf != NULL);

	if (*buf == '~') {
		size_t i = 1;
		char *tilde_dir;

		/* Figure out how much of the string we need to compare. */
		for (; buf[i] != '/' && buf[i] != '\0'; i++) {
			;
		}

		/* Get the home directory. */
		if (i == 1) {
			get_homedir();
			tilde_dir = mallocstrcpy(NULL, homedir.c_str());
		} else {
			const struct passwd *userdata;

			tilde_dir = mallocstrncpy(NULL, buf, i + 1);
			tilde_dir[i] = '\0';

			do {
				userdata = getpwent();
			} while (userdata != NULL && strcmp(userdata->pw_name, tilde_dir + 1) != 0);
			endpwent();
			if (userdata != NULL) {
				tilde_dir = mallocstrcpy(tilde_dir, userdata->pw_dir);
			}
		}

		retval = charalloc(strlen(tilde_dir) + strlen(buf + i) + 1);
		sprintf(retval, "%s%s", tilde_dir, buf + i);

		free(tilde_dir);
	} else {
		retval = mallocstrcpy(NULL, buf);
	}

	return retval;
}

/* Our sort routine for file listings.  Sort alphabetically and
 * case-insensitively, and sort directories before filenames. */
bool sort_directories(const std::string& a, const std::string& b)
{
	struct stat fileinfo;
	bool a_is_dir = (stat(a, &fileinfo) != -1 && S_ISDIR(fileinfo.st_mode));
	bool b_is_dir = (stat(b, &fileinfo) != -1 && S_ISDIR(fileinfo.st_mode));

	if (a_is_dir && !b_is_dir) {
		return true;
	}
	if (!a_is_dir && b_is_dir) {
		return false;
	}

	if (a < b) {
		return true;
	}

	return false;
}

/* Is the given path a directory? */
bool is_dir(const char *buf)
{
	char *dirptr;
	struct stat fileinfo;
	bool retval;

	assert(buf != NULL);

	dirptr = real_dir_from_tilde(buf);

	retval = (stat(dirptr, &fileinfo) != -1 && S_ISDIR(fileinfo.st_mode));

	free(dirptr);

	return retval;
}

/* These functions, username_tab_completion(), cwd_tab_completion()
 * (originally exe_n_cwd_tab_completion()), and input_tab(), were
 * adapted from busybox 0.46 (cmdedit.c).  Here is the notice from that
 * file, with the copyright years updated:
 *
 * Termios command line History and Editting, originally
 * intended for NetBSD sh (ash)
 * Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
 *      Main code:            Adam Rogoyski <rogoyski@cs.utexas.edu>
 *      Etc:                  Dave Cinege <dcinege@psychosis.com>
 *  Majorly adjusted/re-written for busybox:
 *                            Erik Andersen <andersee@debian.org>
 *
 * You may use this code as you wish, so long as the original author(s)
 * are attributed in any redistributions of the source code.
 * This code is 'as is' with no warranty.
 * This code may safely be consumed by a BSD or GPL license. */

/* We consider the first buf_len characters of buf for ~username tab completion. */
std::vector<std::string> username_tab_completion(const char *buf, size_t buf_len)
{
	const struct passwd *userdata;
	std::vector<std::string> matches;

	assert(buf != NULL && buf_len > 0);

	while ((userdata = getpwent()) != NULL) {
		if (strncmp(userdata->pw_name, buf + 1, buf_len - 1) == 0) {
			/* Cool, found a match.  Add it to the list.  This makes a
			 * lot more sense to me (Chris) this way... */

			matches.push_back(std::string("~") + std::string(userdata->pw_name));
		}
	}
	endpwent();

	return matches;
}

/* We consider the first buf_len characters of buf for filename tab completion. */
std::vector<std::string> cwd_tab_completion(const char *buf, bool allow_files, size_t buf_len)
{
	char *dirname = mallocstrcpy(NULL, buf), *filename;
	size_t filenamelen;
	DIR *dir;
	const struct dirent *nextdir;
	std::vector<std::string> matches;

	assert(dirname != NULL);

	null_at(&dirname, buf_len);

	/* Okie, if there's a / in the buffer, strip out the directory part. */
	filename = strrchr(dirname, '/');
	if (filename != NULL) {
		char *tmpdirname = filename + 1;

		filename = mallocstrcpy(NULL, tmpdirname);
		*tmpdirname = '\0';
		tmpdirname = dirname;
		dirname = real_dir_from_tilde(dirname);
		free(tmpdirname);
	} else {
		filename = dirname;
		dirname = mallocstrcpy(NULL, "./");
	}

	assert(dirname[strlen(dirname) - 1] == '/');

	dir = opendir(dirname);

	if (dir == NULL) {
		/* Don't print an error, just shut up and return. */
		beep();
		free(filename);
		free(dirname);
		return matches;
	}

	filenamelen = strlen(filename);

	while ((nextdir = readdir(dir)) != NULL) {
		bool skip_match = false;

		DEBUG_LOG("Comparing name '" << nextdir->d_name << "' against given partial filname '" << buf << "'");
		/* See if this matches. */
		if (strncmp(nextdir->d_name, filename, filenamelen) == 0 &&
		        (*filename == '.' || (strcmp(nextdir->d_name, ".") !=
		                              0 && strcmp(nextdir->d_name, "..") != 0))) {
			/* Cool, found a match.  Add it to the list.  This makes a
			 * lot more sense to me (Chris) this way... */

			char *tmp = charalloc(strlen(dirname) + strlen(nextdir->d_name) + 1);
			sprintf(tmp, "%s%s", dirname, nextdir->d_name);

			/* ...unless the match isn't a directory and allow_files
			 * isn't set, in which case just go to the next match. */
			if (!allow_files && !is_dir(tmp)) {
				skip_match = true;
			}

			free(tmp);

			if (skip_match) {
				continue;
			}

			matches.push_back(std::string(nextdir->d_name));
		}
	}

	closedir(dir);
	free(dirname);
	free(filename);

	return matches;
}

/* Do tab completion.  place refers to how much the statusbar cursor
 * position should be advanced.  refresh_func is the function we will
 * call to refresh the edit window. */
std::string input_tab(const std::string& buf, bool allow_files, size_t *place, bool *lastwastab, void (*refresh_func)(void), bool *list)
{
	char *str = mallocstrcpy(NULL, buf.c_str());
	std::string result = input_tab(str, allow_files, place, lastwastab, refresh_func, list);
	free(str);

	return result;
}

char *input_tab(char *buf, bool allow_files, size_t *place, bool *lastwastab, void (*refresh_func)(void), bool *list)
{
	size_t buf_len;
	std::vector<std::string> matches;

	assert(buf != NULL && place != NULL && *place <= strlen(buf) && lastwastab != NULL && refresh_func != NULL && list != NULL);

	*list = false;

	/* If the word starts with `~' and there is no slash in the word,
	 * then try completing this word as a username. */
	if (*place > 0 && *buf == '~') {
		const char *bob = strchr(buf, '/');

		if (bob == NULL || bob >= buf + *place) {
			matches = username_tab_completion(buf, *place);
		}
	}

	/* Match against files relative to the current working directory. */
	if (matches.size() == 0) {
		matches = cwd_tab_completion(buf, allow_files, *place);
	}

	buf_len = strlen(buf);

	if (matches.size() == 0 || *place != buf_len) {
		beep();
	} else {
		size_t common_len = 0;
		char *mzero;
		const char *lastslash = revstrstr(buf, "/", buf + *place);
		size_t lastslash_len = (lastslash == NULL) ? 0 : lastslash - buf + 1;
		char *match1_mb = charalloc(mb_cur_max() + 1);
		char *match2_mb = charalloc(mb_cur_max() + 1);
		int match1_mb_len, match2_mb_len;

		while (true) {
			size_t match;
			for (match = 1; match < matches.size(); match++) {
				/* Get the number of single-byte characters that all the
				 * matches have in common. */
				match1_mb_len = parse_mbchar(matches[0].c_str() + common_len, match1_mb, NULL);
				match2_mb_len = parse_mbchar(matches[match].c_str() + common_len, match2_mb, NULL);
				match1_mb[match1_mb_len] = '\0';
				match2_mb[match2_mb_len] = '\0';
				if (strcmp(match1_mb, match2_mb) != 0) {
					break;
				}
			}

			if (match < matches.size() || matches[0][common_len] == '\0') {
				break;
			}

			common_len += parse_mbchar(buf + common_len, NULL, NULL);
		}

		free(match1_mb);
		free(match2_mb);

		mzero = charalloc(lastslash_len + common_len + 1);

		strncpy(mzero, buf, lastslash_len);
		strncpy(mzero + lastslash_len, matches[0].c_str(), common_len);

		common_len += lastslash_len;
		mzero[common_len] = '\0';

		assert(common_len >= *place);

		if (matches.size() == 1 && is_dir(mzero)) {
			mzero[common_len++] = '/';

			assert(common_len > *place);
		}

		if (matches.size() > 1 && (common_len != *place || !*lastwastab)) {
			beep();
		}

		/* If there is more of a match to display on the statusbar, show
		 * it.  We reset lastwastab to false: it requires pressing Tab
		 * twice in succession with no statusbar changes to see a match
		 * list. */
		if (common_len != *place) {
			*lastwastab = false;
			buf = charealloc(buf, common_len + buf_len - *place + 1);
			charmove(buf + common_len, buf + *place, buf_len - *place + 1);
			strncpy(buf, mzero, common_len);
			*place = common_len;
		} else if (!*lastwastab || matches.size() < 2) {
			*lastwastab = true;
		} else {
			int longest_name = 0, ncols, editline = 0;

			/* Now we show a list of the available choices. */
			assert(matches.size() > 1);

			/* Sort the list. */
			std::sort(matches.begin(), matches.end(), sort_directories);

			for (auto match : matches) {
				common_len = strnlenpt(match.c_str(), COLS - 1);

				if (common_len > COLS - 1) {
					longest_name = COLS - 1;
					break;
				}

				if (common_len > longest_name) {
					longest_name = common_len;
				}
			}

			assert(longest_name <= COLS - 1);

			/* Each column will be (longest_name + 2) columns wide, i.e.
			 * two spaces between columns, except that there will be
			 * only one space after the last column. */
			ncols = (COLS + 1) / (longest_name + 2);

			/* Blank the edit window, and print the matches out there. */
			blank_edit();
			wmove(edit, 0, 0);

			/* Disable el cursor. */
			curs_set(0);

			for (size_t match = 0; match < matches.size(); match++) {
				char *disp;

				wmove(edit, editline, (longest_name + 2) * (match % ncols));

				if (match % ncols == 0 && editline == editwinrows - 1 && matches.size() - match > ncols) {
					waddstr(edit, _("(more)"));
					break;
				}

				disp = display_string(matches[match].c_str(), 0, longest_name, false);
				waddstr(edit, disp);
				free(disp);

				if ((match + 1) % ncols == 0) {
					editline++;
				}
			}

			wnoutrefresh(edit);
			*list = true;
		}

		free(mzero);
	}

	/* Only refresh the edit window if we don't have a list of filename
	 * matches on it. */
	if (!*list) {
		refresh_func();
	}

	/* Enable el cursor. */
	curs_set(1);

	return buf;
}

/* Only print the last part of a path.  Isn't there a shell command for this? */
std::string tail(const std::string& foo)
{
	return std::string(tail(foo.c_str()));
}

const char *tail(const char *foo)
{
	const char *tmp = strrchr(foo, '/');

	if (tmp == NULL) {
		tmp = foo;
	} else {
		tmp++;
	}

	return tmp;
}

/* Return the constructed dotfile path, or empty if we can't find the home directory. */
std::string construct_filename(const std::string& str)
{
	std::string newstr;

	if (homedir != "") {
		newstr = homedir + str;
	}

	return newstr;
}

std::string histfilename(void)
{
	return construct_filename("/.pinot/search_history");
}

std::string poshistfilename(void)
{
	return construct_filename("/.pinot/filepos_history");
}



void history_error(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	vfprintf(stderr, _(msg), ap);
	va_end(ap);

	fprintf(stderr, _("\nPress Enter to continue\n"));
	while (getchar() != '\n') {
		;
	}

}

/* Now that we have more than one history file, let's just rely
   on a .pinot dir for this stuff.  Return 1 if the dir exists
   or was successfully created, and return 0 otherwise.
 */
int check_dotpinot(void)
{
	struct stat dirstat;
	std::string pinotdir = construct_filename("/.pinot");

	if (stat(pinotdir, &dirstat) == -1) {
		if (mkdir(pinotdir.c_str(), S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
			history_error(N_("Unable to create directory %s: %s\nIt is required for saving/loading search history or cursor position\n"), pinotdir.c_str(), strerror(errno));
			return 0;
		}
	} else if (!S_ISDIR(dirstat.st_mode)) {
		history_error(N_("Path %s is not a directory and needs to be.\npinot will be unable to load or save search or cursor position history\n"), pinotdir.c_str());
		return 0;
	}
	return 1;
}

/* Load search histories from file. */
void load_history(void)
{
	std::string pinothist = histfilename();

	/* Assume do_rcfile() has reported a missing home directory. */
	if (pinothist != "") {
		std::ifstream hist(pinothist, std::ifstream::binary);

		if (!hist) {
			if (errno != ENOENT) {
				/* Don't save history when we quit. */
				UNSET(HISTORYLOG);
				history_error(N_("Error reading %s: %s"), pinothist.c_str(), strerror(errno));
			}
		} else {
			/* Load a history list (first the search history, then the
			 * replace history) from the oldest entry to the newest.
			 * Assume the last history entry is a blank line. */
			filestruct **history = &search_history;
			char *line = NULL;
			size_t buf_len = 0;
			ssize_t read;

			while ((read = getline(&line, &buf_len, hist)) >= 0) {
				if (read > 0 && line[read - 1] == '\n') {
					read--;
					line[read] = '\0';
				}
				if (read > 0) {
					unsunder(line, read);
					update_history(history, line);
				} else {
					history = &replace_history;
				}
			}

			free(line);
			if (search_history->prev != NULL) {
				last_search = mallocstrcpy(NULL, search_history->prev->data);
			}
		}
	}
}

/* Write the lines of a history list, starting with the line at h, to
 * the open file at hist.  Return true if the write succeeded, and false
 * otherwise. */
bool writehist(std::ostream& hist, filestruct *h)
{
	filestruct *p;

	/* Write a history list from the oldest entry to the newest.  Assume
	 * the last history entry is a blank line. */
	for (p = h; p != NULL; p = p->next) {
		size_t p_len = strlen(p->data);

		sunder(p->data);

		hist.write(p->data, p_len);
		hist << "\n";
		if (!hist) {
			return false;
		}
	}

	return true;
}

/* Save histories to ~/.pinot/search_history. */
void save_history(void)
{
	/* Don't save unchanged or empty histories. */
	if (!history_has_changed() || (searchbot->lineno == 1 && replacebot->lineno == 1)) {
		return;
	}

	std::string pinothist = histfilename();

	if (pinothist != "") {
		std::ofstream hist(pinothist, std::ofstream::binary);

		if (!hist) {
			history_error(N_("Error writing %s: %s"), pinothist.c_str(), strerror(errno));
		} else {
			/* Make sure no one else can read from or write to the history file. */
			chmod(pinothist.c_str(), S_IRUSR | S_IWUSR);

			if (!writehist(hist, searchage) || !writehist(hist, replaceage)) {
				history_error(N_("Error writing %s: %s"), pinothist.c_str(), strerror(errno));
			}
		}
	}
}


/* Analogs for the POS history */
void save_poshistory(void)
{
	char *statusstr = NULL;

	std::string poshist = poshistfilename();

	if (poshist != "") {
		std::ofstream hist(poshist, std::ofstream::binary);

		if (!hist) {
			history_error(N_("Error writing %s: %s"), poshist.c_str(), strerror(errno));
		} else {
			/* Make sure no one else can read from or write to the
			 * history file. */
			chmod(poshist.c_str(), S_IRUSR | S_IWUSR);

			for (auto pos : poshistory) {
				hist << pos->filename << " " << pos->lineno << " " << pos->xno << std::endl;
				if (!hist) {
					history_error(N_("Error writing %s: %s"), poshist.c_str(), strerror(errno));
				}
				free(statusstr);
			}
		}
	}
}

/* Update the POS history, given a filename line and column.
 * If no entry is found, add a new entry on the end
 */
void update_poshistory(const std::string& filename, ssize_t lineno, ssize_t xpos)
{
	std::string fullpath = get_full_path(filename);

	for (auto pos : poshistory) {
		if (pos->filename == fullpath) {
			pos->lineno = lineno;
			pos->xno    = xpos;
			return;
		}
	}

	/* Didn't find it, make a new node yo! */

	auto pos = new poshiststruct;
	pos->filename = fullpath;
	pos->lineno   = lineno;
	pos->xno      = xpos;
	poshistory.push_back(pos);
}


/* Check the POS history to see if file matches
 * an existing entry.  If so return 1 and set line and column
 * to the right values  Otherwise return 0
 */
int check_poshistory(const std::string& file, ssize_t *line, ssize_t *column)
{
	auto fullpath = get_full_path(file);

	if (fullpath == "") {
		return 0;
	}

	for (auto pos : poshistory) {
		if (pos->filename == fullpath) {
			*line = pos->lineno;
			*column = pos->xno;
			return 1;
		}
	}
	return 0;
}

/* Load position histories from file. */
void load_poshistory(void)
{
	std::string pinothist = poshistfilename();

	/* Assume do_rcfile() has reported a missing home directory. */
	if (pinothist != "") {
		std::ifstream hist(pinothist, std::ifstream::binary);

		if (!hist) {
			if (errno != ENOENT) {
				/* Don't save history when we quit. */
				UNSET(POS_HISTORY);
				history_error(N_("Error reading %s: %s"), pinothist.c_str(), strerror(errno));
			}
		} else {
			char *line = NULL, *lineptr, *xptr;
			size_t buf_len = 0;
			ssize_t read, lineno, xno;

			/* See if we can find the file we're currently editing */
			while ((read = getline(&line, &buf_len, hist)) >= 0) {
				if (read > 0 && line[read - 1] == '\n') {
					read--;
					line[read] = '\0';
				}
				if (read > 0) {
					unsunder(line, read);
				}
				lineptr = parse_next_word(line);
				xptr = parse_next_word(lineptr);
				lineno = atoi(lineptr);
				xno = atoi(xptr);

				auto pos = new poshiststruct;
				pos->filename = mallocstrcpy(NULL, line);
				pos->lineno   = lineno;
				pos->xno      = xno;
				poshistory.push_back(pos);
			}

			free(line);
		}
	}
}
