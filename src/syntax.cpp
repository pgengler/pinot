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
	regcomp(ext, ext_regex.c_str(), REG_EXTENDED);
}

bool SyntaxMatch::matches(const char *str) const
{
	DEBUG_LOG("Matching regex \"" << ext_regex << "\" against \"" << str << '"');

	return (regexec(ext, str, 0, NULL, 0) == 0);
}

/***********************************/

Syntax::Syntax(const char *desc)
: nmultis(0)
{
	this->desc = std::string(desc);
}

void Syntax::add_color(ColorPtr color)
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
