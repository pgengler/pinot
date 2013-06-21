#include "syntax.h"

SyntaxMatch::SyntaxMatch(const char *str)
: ext_regex(str), ext(NULL)
{
	compile();
}

SyntaxMatch::~SyntaxMatch()
{
	if (ext) {
		regfree(ext);
		delete ext;
	}
}

void SyntaxMatch::compile()
{
	ext = new regex_t;
	regcomp(ext, fixbounds(ext_regex), REG_EXTENDED);
}

bool SyntaxMatch::matches(const char *str) const
{
#ifdef DEBUG
	fprintf(stderr, "Matching regex \"%s\" against \"%s\"\n", ext_regex, str);
#endif /* DEBUG */

	return (regexec(ext, str, 0, NULL, 0) == 0);
}

/***********************************/

void Syntax::add_color(colortype *color)
{
	_colors.push_back(color);
}

ColorList Syntax::colors() const
{
	return _colors;
}
