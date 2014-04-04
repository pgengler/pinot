#include "Keyboard.h"

#include "proto.h"

Keyboard::Keyboard()
{
	termkey = termkey_new(0, TERMKEY_FLAG_NOTERMIOS|TERMKEY_FLAG_CONVERTKP);
}

bool Keyboard::has_input() const
{
	return (termkey->buffcount > 0);
}

Key Keyboard::get_key()
{
	TermKeyKey key;
	if (termkey_waitkey(termkey, &key) != TERMKEY_RES_KEY) {
		throw "Error reading keyboard input!";
	}
	return Key(termkey, key);
}

Key::Key(TermKey* termkey, TermKeyKey key)
: termkey(termkey), key(key)
{

}

std::string Key::format()
{
	char keybuffer[50];
	TermKeyFormat format = static_cast<TermKeyFormat>(TERMKEY_FORMAT_ALTISMETA | TERMKEY_FORMAT_CARETCTRL);
	termkey_strfkey(termkey, keybuffer, sizeof(keybuffer), &key, format);

	return std::string(keybuffer);
}

Key::operator std::string()
{
	if (key.type == TERMKEY_TYPE_UNICODE) {
		return std::string(key.utf8);
	}
	return "";
}

bool Key::is_ascii_control_char()
{
	return is_ascii_cntrl_char(std::string(*this)[0]);
}

bool Key::is_escape_key()
{
	return (key.type == TERMKEY_TYPE_KEYSYM && key.code.sym == TERMKEY_SYM_ESCAPE);
}

bool Key::has_meta_key()
{
	return (key.modifiers & TERMKEY_KEYMOD_ALT);
}
