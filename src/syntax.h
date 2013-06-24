#pragma once

#include <list>
#include <string>
#include <unordered_map>

#include <pcreposix.h>

#include "macros.h"

#define COLORWIDTH short
typedef struct colortype {
	colortype() : pairnum(0) { }

	COLORWIDTH fg;
	/* This syntax's foreground color. */
	COLORWIDTH bg;
	/* This syntax's background color. */
	bool bright;
	/* Is this color A_BOLD? */
	bool underline;
	/* Is this color A_UNDERLINE? */
	bool icase;
	/* Is this regex string case insensitive? */
	int pairnum;
	/* The color pair number used for this foreground color and
	 * background color. */
	char *start_regex;
	/* The start (or all) of the regex string. */
	regex_t *start;
	/* The compiled start (or all) of the regex string. */
	char *end_regex;
	/* The end (if any) of the regex string. */
	regex_t *end;
	/* The compiled end (if any) of the regex string. */
	int id;
	/* basic id for assigning to lines later */
} colortype;
typedef std::list<colortype *> ColorList;

class SyntaxMatch {
	public:
		SyntaxMatch(const char*);
		virtual ~SyntaxMatch();

		bool matches(const char*) const;
	private:
		void compile();

		std::string ext_regex;
		/* The extensions that match this syntax. */

		regex_t *ext;
		/* The compiled extensions that match this syntax. */
};
typedef std::list<SyntaxMatch *> SyntaxMatchList;

class Syntax {
	public:
		std::string desc;
		/* The name of this syntax. */

		SyntaxMatchList extensions;
		/* The list of extensions that this syntax applies to. */

		SyntaxMatchList headers;
		/* Regexes to match on the 'header' (1st line) of the file */

		SyntaxMatchList magics;
		/* Regexes to match libmagic results */

		int nmultis;
		/* How many multi line strings this syntax has */

		std::list<std::string> extends;
		/* Names of other syntaxes which this one extends */

		ColorList colors() const;
		ColorList own_colors() const;
		void add_color(colortype*);

	private:
		ColorList _colors;
		/* The colors used in this syntax. */
};
typedef std::unordered_map<std::string, Syntax *> SyntaxMap;

#define CNONE 		(1<<1)
/* Yay, regex doesn't apply to this line at all! */
#define CBEGINBEFORE 	(1<<2)
/* regex starts on an earlier line, ends on this one */
#define CENDAFTER 	(1<<3)
/* regex sraers on this line and ends on a later one */
#define CWHOLELINE 	(1<<4)
/* whole line engulfed by the regex  start < me, end > me */
#define CSTARTENDHERE 	(1<<5)
/* regex starts and ends within this line */
#define CWTF		(1<<6)
/* Something else */

extern SyntaxMap syntaxes;
extern const char *fixbounds(const std::string& r);
