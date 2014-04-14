#include "Keyboard.h"

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

std::string Key::verbatim()
{
	if (key.type == TERMKEY_TYPE_UNICODE) {
		if (has_control_key()) {
			return control_char(key.utf8[0]);
		} else {
			return std::string(key.utf8);
		}
	}
	return "";
}

bool Key::has_control_key()
{
	return (key.modifiers & TERMKEY_KEYMOD_CTRL);
}

bool Key::has_meta_key()
{
	return (key.modifiers & TERMKEY_KEYMOD_ALT);
}

std::string Key::control_char(char c) const
{
	char control_char = (tolower(c) - 'a') + 1;
	return std::string(&control_char, 1);
}
