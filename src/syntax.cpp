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
	DEBUG_LOG("Matching regex \"%s\" against \"%s\"\n", ext_regex, str);

	return (regexec(ext, str, 0, NULL, 0) == 0);
}

/***********************************/

void Syntax::add_color(colortype *color)
{
	_colors.push_back(color);
}

ColorList Syntax::colors() const
{
	ColorList all_colors;

	for (auto syntax_name : extends) {
		Syntax *syntax = syntaxes[syntax_name];
		if (!syntax) {
			// TODO: catch invalid syntax name
			continue;
		}
		for (auto color : syntax->colors()) {
			all_colors.push_back(color);
		}
	}

	// Lastly add the specific colors
	for (auto color : _colors) {
		all_colors.push_back(color);
	}

	return all_colors;
}

ColorList Syntax::own_colors() const
{
	return _colors;
}
