#pragma once

// include ncurses for ESCDELAY
#include <ncurses.h>

// need to include "termkey-internal.h" because that actually defines
// the TermKeyStruct and we get compiler errors without it
#include "termkey-internal.h"
#include "termkey.h"

#include "String.h"
using pinot::string;

class Key
{
	public:
		Key(TermKey* termkey, TermKeyKey key);

		string format();
		operator string();
		string verbatim();

		bool has_control_key();
		bool has_meta_key();
	private:
		string control_char(char c) const;

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
