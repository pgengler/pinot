/**************************************************************************
 *   proto.h                                                              *
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

#pragma once

#include "pinot.h"

/* All external variables.  See global.c for their descriptions. */
extern sigjmp_buf jump_buf;
extern bool jump_buf_set;

extern Keyboard *keyboard;

extern ssize_t fill;
extern ssize_t wrap_at;

extern std::string last_search;
extern std::string last_replace;

extern unsigned flags[4];
extern WINDOW *topwin;
extern WINDOW *edit;
extern WINDOW *bottomwin;
extern int editwinrows;
extern int maxrows;

extern filestruct *cutbuffer;
extern filestruct *cutbottom;
extern partition *filepart;
extern std::list<OpenFile> openfiles;
extern std::list<OpenFile>::iterator openfile;

extern char *matchbrackets;

extern std::string whitespace;
extern int whitespace_len[2];

extern bool nodelay_mode;
extern std::string answer;

extern ssize_t tabsize;

extern std::string backup_dir;
extern const std::string locking_prefix;
extern const std::string locking_suffix;

extern char *alt_speller;

extern std::list<sc*> sclist;
extern std::list<subnfunc*> allfuncs;
extern SyntaxMap syntaxes;
extern std::string syntaxstr;

extern bool edit_refresh_needed;
extern int currmenu;

extern filestruct *search_history;
extern filestruct *searchage;
extern filestruct *searchbot;
extern filestruct *replace_history;
extern filestruct *replaceage;
extern filestruct *replacebot;
extern std::list<poshiststruct *> poshistory;
void update_poshistory(const std::string& filename, ssize_t lineno, ssize_t xpos);

extern regex_t search_regexp;
extern regmatch_t regmatches[10];

extern int highlight_attribute;
extern std::string specified_color_combo[NUMBER_OF_ELEMENTS];
extern ColorPair interface_colors[NUMBER_OF_ELEMENTS];

extern std::string homedir;

/* All functions in browser.c. */
std::string do_browser(std::string path, DIR *dir);
std::string do_browse_from(const std::string& inpath);
void browser_init(const std::string& path, DIR *dir);
void browser_refresh(void);
bool browser_select_filename(const std::string& needle);
bool filesearch_init(void);
void findnextfile(const std::string& needle);
void filesearch_abort(void);
void do_filesearch(void);
void do_fileresearch(void);
void do_first_file(void);
void do_last_file(void);
std::string striponedir(const std::string& path);

/* All functions in chars.c. */
char *addstrings(char *str1, size_t len1, char *str2, size_t len2);
void utf8_init(void);
bool using_utf8(void);
#ifndef HAVE_ISBLANK
bool nisblank(int c);
#endif
#ifndef HAVE_ISWBLANK
bool niswblank(wchar_t wc);
#endif
bool is_byte(int c);
bool is_alnum_mbchar(const char *c);
bool is_blank_mbchar(const char *c);
bool is_ascii_cntrl_char(int c);
bool is_cntrl_char(int c);
bool is_cntrl_wchar(wchar_t wc);
bool is_cntrl_mbchar(const char *c);
bool is_punct_mbchar(const char *c);
bool is_word_mbchar(const char *c, bool allow_punct);
char control_rep(char c);
wchar_t control_wrep(wchar_t c);
char *control_mbrep(const char *c, char *crep, int *crep_len);
char *mbrep(const char *c, char *crep, int *crep_len);
int mbwidth(const char *c);
int mb_cur_max(void);
char *make_mbchar(long chr, int *chr_mb_len);
int parse_mbchar(const char *buf, char *chr, size_t *col);
size_t move_mbleft(const std::string& str, size_t pos);
size_t move_mbleft(const char *buf, size_t pos);
size_t move_mbright(const std::string& str, size_t pos);
size_t move_mbright(const char *buf, size_t pos);
#ifndef HAVE_STRNCASECMP
int nstrncasecmp(const char *s1, const char *s2, size_t n);
#endif
int mbstrncasecmp(const char *s1, const char *s2, size_t n);
#ifndef HAVE_STRCASESTR
char *nstrcasestr(const char *haystack, const char *needle);
#endif
char *mbstrcasestr(const char *haystack, const char *needle);
char *revstrstr(const char *haystack, const char *needle, const char *rev_start);
char *revstrcasestr(const char *haystack, const char *needle, const char *rev_start);
char *mbrevstrcasestr(const char *haystack, const char *needle, const char *rev_start);
size_t mbstrlen(const char *s);
#ifndef HAVE_STRNLEN
size_t nstrnlen(const char *s, size_t maxlen);
#endif
size_t mbstrnlen(const char *s, size_t maxlen);
char *mbstrchr(const char *s, const char *c);
char *mbstrpbrk(const char *s, const char *accept);
char *revstrpbrk(const char *s, const char *accept, const char *rev_start);
char *mbrevstrpbrk(const char *s, const char *accept, const char *rev_start);
bool has_blank_chars(const char *s);
bool has_blank_mbchars(const char *s);
bool is_valid_unicode(wchar_t wc);
bool is_valid_mbstring(const char *s);

/* All functions in color.c. */
void set_colorpairs(void);
void color_init(void);
void color_update(void);

/* All functions in cut.c. */
void cutbuffer_reset(void);
bool keeping_cutbuffer(void);
void cut_line(void);
void cut_marked(void);
void cut_to_eol(void);
void cut_to_eof(void);
void do_cut_text(bool copy_text, bool cut_till_eof, bool undoing);
void do_cut_text_void(void);
void do_copy_text(void);
void do_cut_till_eof(void);
void do_uncut_text(void);

/* All functions in files.c. */
void make_new_buffer(void);
void initialize_buffer(void);
void initialize_buffer_text(void);
void open_buffer(std::string filename, bool undoable);
void replace_buffer(const std::string& filename);
void display_buffer(void);
std::list<OpenFile>::iterator switch_to_prevnext_buffer(bool next, bool quiet=false);
void switch_to_prev_buffer_void(void);
void switch_to_next_buffer_void(void);
bool close_buffer(bool quiet);
filestruct *read_line(char *buf, filestruct *prevnode, bool *first_line_ins, size_t buf_len);
void read_file(FILE *f, int fd, const std::string& filename, bool undoable, bool checkwritable);
int open_file(const std::string& filename, bool newfie, bool quiet, FILE **f);
std::string get_next_filename(const std::string& name, const std::string& suffix);
void do_insertfile(bool execute);
void do_insertfile_void(void);
void do_execute_command();
std::string get_full_path(const std::string& origpath);
std::string check_writable_directory(const std::string& path);
std::string safe_tempfile(FILE **f);
void init_backup_dir(void);
int delete_lockfile(const std::string& lockfilename);
int write_lockfile(const std::string& lockfilename, const std::string& origfilename, bool modified);
int copy_file(FILE *inn, FILE *out);
bool write_file(const std::string& name, FILE *f_open, bool tmp, AppendType append, bool nonamechange);
bool write_marked_file(const std::string& name, FILE *f_open, bool tmp, AppendType append);
bool do_writeout(bool exiting);
void do_writeout_void(void);
std::string real_dir_from_tilde(const std::string& buf);
char *real_dir_from_tilde(const char *buf);
bool sort_directories(const std::string& a, const std::string& b);
bool is_dir(const char *buf);
std::vector<std::string> username_tab_completion(const char *buf, size_t buf_len);
std::vector<std::string> cwd_tab_completion(const char *buf, bool allow_files, size_t buf_len);
std::string input_tab(const std::string& buf, bool allow_files, size_t *place, bool *lastwastab, void (*refresh_func)(void), bool *list);
char *input_tab(char *buf, bool allow_files, size_t *place, bool *lastwastab, void (*refresh_func)(void), bool *list);
std::string tail(const std::string& foo);
const char *tail(const char *foo);
std::string histfilename(void);
void load_history(void);
bool writehist(std::ostream& hist, filestruct *histhead);
void save_history(void);
int check_dotpinot(void);
void load_poshistory(void);
void save_poshistory(void);
int check_poshistory(const std::string& file, ssize_t *line, ssize_t *column);

/* All functions in global.c. */
size_t length_of_list(int menu);
void shortcut_init(void);
void set_lint_or_format_shortcuts(void);
void set_spell_shortcuts(void);
#ifdef DEBUG
void thanks_for_all_the_fish(void);
#endif

/* All functions in help.c. */
void do_help_void(void);
void do_help(void (*refresh_func)(void));
void help_init(void);
size_t help_line_len(const char *ptr);

/* All functions in move.c. */
void do_first_line(void);
void do_last_line(void);
void do_page_up(void);
void do_page_down(void);
bool do_next_word(bool allow_punct, bool allow_update);
void do_next_word_void(void);
bool do_prev_word(bool allow_punct, bool allow_update);
void do_prev_word_void(void);
void do_home(void);
void do_end(void);
void do_up(bool scroll_only);
void do_up_void(void);
void do_scroll_up(void);
void do_down(bool scroll_only);
void do_down_void(void);
void do_scroll_down(void);
void do_left(void);
void do_right(void);

/* All functions in pinot.c. */
filestruct *make_new_node(filestruct *prevnode);
filestruct *copy_node(const filestruct *src);
void splice_node(filestruct *begin, filestruct *newnode, filestruct *end);
void unlink_node(const filestruct *fileptr);
void delete_node(filestruct *fileptr);
filestruct *copy_filestruct(const filestruct *src);
void free_filestruct(filestruct *src);
void renumber(filestruct *fileptr);
partition *partition_filestruct(filestruct *top, size_t top_x, filestruct *bot, size_t bot_x);
void unpartition_filestruct(partition **p);
void move_to_filestruct(filestruct **file_top, filestruct **file_bot, filestruct *top, size_t top_x, filestruct *bot, size_t bot_x);
void copy_from_filestruct(filestruct *some_buffer);
void print_view_warning(void);
void finish(void);
void die(const char *msg, ...);
void die_save_file(std::string die_filename, struct stat *die_stat);
void window_init(void);
void print_opt_full(const char *shortflag
#ifdef HAVE_GETOPT_LONG
                    , const char *longflag
#endif
                    , const char *desc);
void usage(void);
void version(void);
int more_space(void);
int no_help(void);
void pinot_disabled_msg(void);
void do_exit(void);
void signal_init(void);
void handle_hupterm(int signal);
void do_suspend(int signal);
void do_continue(int signal);
void handle_sigwinch(int signal);
void allow_pending_sigwinch(bool allow);
void do_toggle(int flag);
void do_toggle_void(void);
void disable_extended_io(void);
#ifdef USE_SLANG
void disable_signals(void);
#endif
void enable_signals(void);
void disable_flow_control(void);
void enable_flow_control(void);
void terminal_init(void);
void do_input(void);
void do_output(const std::string& output, bool allow_cntrls);
void do_output(char *output, size_t output_len, bool allow_cntrls);

/* All functions in prompt.c. */
Key do_statusbar_input(bool *ran_func, bool *finished, void (*refresh_func)(void));
void do_statusbar_output(std::string output, bool allow_cntrls);
void do_statusbar_output(char *output, size_t output_len, bool allow_cntrls);
void do_statusbar_home(void);
void do_statusbar_end(void);
void do_statusbar_left(void);
void do_statusbar_right(void);
void do_statusbar_backspace(void);
void do_statusbar_delete(void);
void do_statusbar_cut_text(void);
bool do_statusbar_next_word(bool allow_punct);
bool do_statusbar_prev_word(bool allow_punct);
void do_statusbar_verbatim_input(void);
size_t statusbar_xplustabs(void);
size_t get_statusbar_page_start(size_t start_col, size_t column);
void reset_statusbar_cursor(void);
void update_statusbar_line(const std::string& curranswer, size_t index);
bool need_statusbar_horizontal_update(size_t pww_save);
void total_statusbar_refresh(void (*refresh_func)(void));
FunctionPtr get_prompt_string(std::shared_ptr<Key>& value, bool allow_tabs, bool allow_files, bool *list, const std::string& curranswer, filestruct **history_list, void (*refresh_func)(void));
PromptResult do_prompt(bool allow_tabs, bool allow_files, int menu, std::shared_ptr<Key>& key, const std::string& curranswer, filestruct **history_list, void (*refresh_func)(void), const char *msg, ...);
void do_prompt_abort(void);
YesNoPromptResult do_yesno_prompt(bool all, const char *msg);

/* All functions in rcfile.c. */
void rcfile_error(const char *msg, ...);
char *parse_next_word(char *ptr);
char *parse_argument(char *ptr);
char *parse_next_regex(char *ptr);
bool nregcomp(const char *regex, int cflags);
void parse_syntax(char *ptr);
void parse_extends(char *ptr);
void parse_magic_syntax(char *ptr);
void parse_include(char *ptr);
COLORWIDTH color_name_to_value(std::string colorname, bool *bright, bool *underline);
void parse_colors(char *ptr, bool icase);
bool parse_color_names(const std::string& combostr, short *fg, short *bg, bool *bright, bool *underline);
void reset_multis(filestruct *fileptr, bool force);
void alloc_multidata_if_needed(filestruct *fileptr);
void parse_rcfile(std::ifstream &rcstream, bool syntax_only);
void do_rcfile(void);

/* All functions in search.c. */
bool regexp_init(const char *regexp);
void regexp_cleanup(void);
void not_found_msg(const std::string& str);
void not_found_msg(const char *str);
void search_replace_abort(void);
int search_init(bool replacing, bool use_answer);
bool findnextstr(bool whole_word, bool no_sameline, const filestruct *begin, size_t begin_x, const std::string& needle, size_t *needle_len);
bool findnextstr(bool whole_word, bool no_sameline, const filestruct *begin, size_t begin_x, const char *needle, size_t *needle_len);
void findnextstr_wrap_reset(void);
void do_search(void);
void do_research(void);
int replace_regexp(char *string, bool create);
char *replace_line(const char *needle);
ssize_t do_replace_loop(bool whole_word, bool *canceled, const filestruct *real_current, size_t *real_current_x, const char *needle);
void do_replace(void);
void do_gotolinecolumn(ssize_t line, ssize_t column, bool use_answer, bool interactive, bool save_pos, bool allow_update);
void do_gotolinecolumn_void(void);
void do_gotopos(ssize_t pos_line, size_t pos_x, ssize_t pos_y, size_t pos_pww);
void goto_line_posx(ssize_t line, size_t pos_x);
bool find_bracket_match(bool reverse, const char *bracket_set);
void do_find_bracket(void);
bool history_has_changed(void);
void history_init(void);
void history_reset(const filestruct *h);
filestruct *find_history(const filestruct *h_start, const filestruct *h_end, const char *s, size_t len);
void update_history(filestruct **h, const std::string& s);
void update_history(filestruct **h, const char *s);
char *get_history_older(filestruct **h);
char *get_history_newer(filestruct **h);
void get_history_older_void(void);
void get_history_newer_void(void);
std::string get_history_completion(filestruct **h, const std::string& s, size_t len);
char *get_history_completion(filestruct **h, const char *s, size_t len);

/* All functions in text.c. */
void do_mark(void);
void do_delete(void);
void do_backspace(void);
void do_tab(void);
void do_indent(ssize_t cols);
void do_indent_void(void);
void do_unindent(void);
void do_undo(void);
void do_redo(void);
void do_enter(bool undoing);
void do_enter_void(void);
void cancel_command(int signal);
bool execute_command(const std::string& command);
int execute_command_silently(const std::string& command);
void wrap_reset(void);
bool do_wrap(filestruct *line);
ssize_t break_line(const char *line, ssize_t goal, bool newln);
size_t indent_length(const std::string& line);
bool do_int_spell_fix(const char *word);
const char *do_int_speller(const char *tempfile_name);
const char *do_alt_speller(char *tempfile_name);
void do_spell(void);
void do_linter(void);
void do_formatter(void);
void do_wordlinechar_count(void);
void do_verbatim_input(void);

/* All functions in utils.c. */
int digits(size_t n);
void get_homedir(void);
bool parse_num(const char *str, ssize_t *val);
bool parse_line_column(const std::string& str, ssize_t *line, ssize_t *column);
bool parse_line_column(const char *str, ssize_t *line, ssize_t *column);
void align(char **str);
void null_at(char **data, size_t index);
void unsunder(std::string& str);
void unsunder(char *str, size_t true_len);
void sunder(std::string& str);
void sunder(char *str);
#ifndef HAVE_GETLINE
ssize_t ngetline(char **lineptr, size_t *n, FILE *stream);
#endif
#ifndef HAVE_GETDELIM
ssize_t ngetdelim(char **lineptr, size_t *n, int delim, FILE *stream);
#endif
ssize_t getdelim(char **lineptr, size_t *n, char delim, std::istream &stream);
ssize_t getline(char **lineptr, size_t *n, std::istream &stream);
bool regexp_bol_or_eol(const regex_t *preg, const char *string);
bool is_whole_word(size_t pos, const char *buf, const char *word);
const char *strstrwrapper(const char *haystack, const char *needle, const char *start);
void nperror(const char *s);
void *nmalloc(size_t howmuch);
void *nrealloc(void *ptr, size_t howmuch);
char *mallocstrncpy(char *dest, const char *src, size_t n);
char *mallocstrcpy(char *dest, const char *src);
size_t get_page_start(size_t column);
size_t xplustabs(void);
size_t actual_x(const char *s, size_t column);
size_t strnlenpt(const char *s, size_t maxlen);
size_t strlenpt(const char *s);
void new_magicline(void);
void remove_magicline(void);
void mark_order(const filestruct **top, size_t *top_x, const filestruct **bot, size_t *bot_x, bool *right_side_up);
void add_undo(UndoType current_action);
void update_undo(UndoType action);
size_t get_totsize(const filestruct *begin, const filestruct *end);
filestruct *fsfromline(ssize_t lineno);
#ifdef DEBUG
void dump_filestruct(const filestruct *inptr);
void dump_filestruct_reverse(void);
#endif

/* All functions in winio.c. */
Key get_kbinput(WINDOW *win);
std::string get_verbatim_kbinput(WINDOW *win);
const sc *get_shortcut(Key kbinput);
const sc *first_sc_for(int menu, void (*func)(void));
void blank_line(WINDOW *win, int y, int x, int n);
void blank_titlebar(void);
void blank_topbar(void);
void blank_edit(void);
void blank_statusbar(void);
void blank_bottombars(void);
void check_statusblank(void);
std::string display_string(const std::string& buf, size_t start_col, size_t len, bool dollars);
char *display_string(const char *buf, size_t start_col, size_t len, bool dollars);
void titlebar(const std::string& path);
void titlebar(const char *path);
void set_modified(void);
void statusbar(const char *msg, ...);
void bottombars(int menu);
void onekey(const std::string& keystroke, const std::string& desc, size_t len);
void reset_cursor(void);
void edit_draw(filestruct *fileptr, const char *converted, int line, size_t start);
int update_line(filestruct *fileptr, size_t index);
bool need_horizontal_update(size_t pww_save);
bool need_vertical_update(size_t pww_save);
void edit_scroll(ScrollDir direction, ssize_t nlines);
void edit_redraw(filestruct *old_current, size_t pww_save);
void edit_refresh(void);
void edit_update(UpdateType location);
void total_redraw(void);
void total_refresh(void);
void display_main_list(void);
void do_cursorpos(bool constant);
void do_cursorpos_void(void);
void do_replace_highlight(bool highlight, const char *word);
const char *flagtostr(int flag);
const subnfunc *sctofunc(sc *s);
FunctionPtr func_from_key(const Key& kbinput);
void empty_sclist(void);
void print_sclist(void);
sc *strtosc(std::string input);
int strtomenu(std::string input);
void xon_complaint(void);
void xoff_complaint(void);
void do_suspend_void(void);
void set_color(WINDOW *win, ColorPair color);
void clear_color(WINDOW *win, ColorPair color);

void enable_nodelay(void);
void disable_nodelay(void);

/* May as just throw these here since they are just placeholders */
void do_cancel(void);
void do_page_up(void);
void do_page_down(void);
void case_sens_void(void);
void regexp_void(void);
void gototext_void(void);
void to_files_void(void);
void dos_format_void(void);
void mac_format_void(void);
void append_void(void);
void prepend_void(void);
void backup_file_void(void);
void new_buffer_void(void);
void backwards_void(void);
void goto_dir_void(void);
void toggle_replace_void(void);
void toggle_execute_void(void);
