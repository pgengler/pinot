#pragma once

#include <string>

// include ncurses for ESCDELAY
#include <ncurses.h>

// need to include "termkey-internal.h" because that actually defines
// the TermKeyStruct and we get compiler errors without it
#include "termkey-internal.h"
#include "termkey.h"

class Key
{
	public:
		Key(TermKey* termkey, TermKeyKey key);

		std::string format();
		operator std::string();
		std::string verbatim();

		bool has_control_key();
		bool has_meta_key();
	private:
		std::string control_char(char c) const;

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
