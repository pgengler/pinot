#pragma once

#include <list>
#include <string>

#include <pcreposix.h>

extern const char *fixbounds(const std::string& r);

#define COLORWIDTH short
typedef struct colortype {
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
	struct colortype *next;
	/* Next set of colors. */
	int id;
	/* basic id for assigning to lines later */
} colortype;

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

typedef struct syntaxtype {
	std::string desc;
	/* The name of this syntax. */
	SyntaxMatchList extensions;
	/* The list of extensions that this syntax applies to. */
	SyntaxMatchList headers;
	/* Regexes to match on the 'header' (1st line) of the file */
	SyntaxMatchList magics;
	/* Regexes to match libmagic results */
	colortype *color;
	/* The colors used in this syntax. */
	int nmultis;
	/* How many multi line strings this syntax has */
} syntaxtype;
typedef std::list<syntaxtype *> SyntaxList;

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
