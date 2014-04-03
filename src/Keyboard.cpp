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
	return format();
}
