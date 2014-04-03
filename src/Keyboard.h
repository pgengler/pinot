#pragma once

#include <string>

// need to include "termkey-internal.h" because that actually defines
// the TermKeyStruct and we get compiler errord without it
#include "termkey-internal.h"
#include "termkey.h"

class Key
{
	public:
		Key(TermKey* termkey, TermKeyKey key);

		std::string format();
		operator std::string();
	private:
		TermKey *termkey;
		TermKeyKey key;
};

class Keyboard
{
	public:
		Keyboard();

		bool has_input() const;
		Key get_key();
	private:
		TermKey *termkey;
};
