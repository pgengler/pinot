#pragma once

#include <string>

// need to include "termkey-internal.h" because that actually defines
// the TermKeyStruct and we get compiler errord without it
#include "termkey-internal.h"
#include "termkey.h"

class Keyboard
{
	public:
		Keyboard();

		bool has_input() const;
		TermKeyKey get_key();
		std::string format_key(TermKeyKey key);

	private:
		TermKey *termkey;
};
