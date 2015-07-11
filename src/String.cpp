#include "String.h"

#include <string>

namespace pinot
{
	character::character(char ch)
	: str(&ch, 1)
	{

	}

	character::character(const icu::UnicodeString& s)
	: str(s)
	{

	}

	bool character::is_blank() const
	{
		return u_isblank(str[0]);
	}

	bool character::operator==(char ch)
	{
		std::string s;
		str.toUTF8String(s);
		return s[0] == ch;
	}

	bool character::operator!=(char ch)
	{
		std::string s;
		str.toUTF8String(s);
		return s[0] != ch;
	}

	std::ostream& operator<<(std::ostream& stream, const character& ch)
	{
		stream << ch.str;
		return stream;
	}
}


namespace pinot {
	string::string()
	: unicode_str("")
	{

	}

	string::string(char ch)
	: unicode_str(ch)
	{

	}

	string::string(const character& ch)
	: unicode_str(ch.str)
	{

	}

	string::string(const char *str)
	: unicode_str(str)
	{

	}

	string::string(const icu::UnicodeString& str)
	: unicode_str(str)
	{

	}

	string::string(const std::string& str)
	: unicode_str(str.c_str())
	{

	}

	string::string(const string& str)
	: unicode_str(str.unicode_str)
	{

	}

	string::~string()
	{
		if (c_str_buffer) {
			delete[] c_str_buffer;
		}
	}

	string& string::append(const char *str, size_t chars)
	{
		unicode_str.append(reinterpret_cast<const UChar*>(str), chars);
		return *this;
	}

	string& string::remove(size_t start, size_t num_to_remove)
	{
		unicode_str.remove(start, num_to_remove);
		return *this;
	}

	bool string::compare(size_t pos, size_t len, const string& str) const
	{
		return substr(pos, len) == str;
	}

	bool string::starts_with(const string& str) const
	{
		return unicode_str.startsWith(str.unicode_str);
	}

	const char* string::c_str()
	{
		std::string str;
		unicode_str.toUTF8String(str);
		const char *str_c = str.c_str();
		if (c_str_buffer) {
			delete[] c_str_buffer;
		}
		c_str_buffer = new char[::strlen(str_c) + 1];
		::strcpy(c_str_buffer, str_c);

		return c_str_buffer;
	}

	bool string::empty() const
	{
		return length() == 0;
	}

	ssize_t string::length() const
	{
		return unicode_str.length();
	}

	std::string string::str() const
	{
		return std::string(reinterpret_cast<const char *>(unicode_str.getBuffer()), unicode_str.length());
	}

	character string::front() const
	{
		return operator[](0);
	}

	character string::back() const
	{
		return operator[](length()-1);
	}

	string string::substr(size_t pos, size_t len) const
	{
		if (len == npos) {
			len = length() - pos;
		}
		icu::UnicodeString substring(unicode_str, pos, len);
		return string(substring);
	}

	string string::to_lower() const
	{
		auto new_str = unicode_str;
		new_str.toLower();
		return string(new_str);
	}

	//-----------------------------

	ssize_t string::find(const string& other) const
	{
		auto index = unicode_str.indexOf(other.unicode_str);
		return (index != -1) ? index : npos;
	}

	ssize_t string::rfind(const string& other) const
	{
		auto index = unicode_str.lastIndexOf(other.unicode_str);
		return (index != -1) ? index : npos;
	}

	ssize_t string::find_first_of(const string& other) const
	{
		ssize_t first = npos;
		for (auto ch : other) {
			auto pos = find(ch);
			if (pos < first) {
				first = pos;
			}
		}
		return first;
	}

	//-----------------------------

	character string::operator[](size_t index) const
	{
		return character(substr(index, 1).unicode_str);
	}

	string& string::replace(size_t pos, char c)
	{
		unicode_str.replace(pos, 1, c);
		return *this;
	}

	//-----------------------------

	string& string::operator+=(const string& other)
	{
		unicode_str.append(other.unicode_str);
		return *this;
	}

	//-----------------------------

	bool string::operator==(const string& other) const
	{
		return unicode_str == other.unicode_str;
	}

	bool string::operator!=(const string& other) const
	{
		return unicode_str != other.unicode_str;
	}

	bool string::operator==(const char *other) const
	{
		return unicode_str == icu::UnicodeString(other);
	}

	bool string::operator!=(const char *other) const
	{
		return unicode_str != icu::UnicodeString(other);
	}

	bool string::operator<(const string& other) const
	{
		return unicode_str < other.unicode_str;
	}

	bool string::operator<=(const string& other) const
	{
		return unicode_str <= other.unicode_str;
	}

	bool string::operator>(const string& other) const
	{
		return unicode_str > other.unicode_str;
	}

	bool string::operator>=(const string& other) const
	{
		return unicode_str >= other.unicode_str;
	}

	//-----------------------------

	int32_t string::hash_code() const
	{
		return unicode_str.hashCode();
	}

	//-----------------------------

	std::istream& getline(std::istream& stream, string& str)
	{
		std::string hack;
		std::getline(stream, hack);
		str.unicode_str = icu::UnicodeString::fromUTF8(icu::StringPiece(hack.c_str()));
		return stream;
	}

	std::istream& operator>>(std::istream& stream, string& str)
	{
		stream >> str.unicode_str;
		return stream;
	}

	std::ostream& operator<<(std::ostream& stream, const string& str)
	{
		stream << str.unicode_str;
		return stream;
	}

	//-----------------------------

	string operator+(string a, const string& b)
	{
		a.unicode_str.append(b.unicode_str);
		return a;
	}

	//-----------------------------

	string::iterator string::begin() const
	{
		return iterator(unicode_str, 0);
	}

	string::iterator string::end() const
	{
		return iterator(unicode_str, length());
	}

	//-----------------------------

	string::iterator::iterator(const icu::UnicodeString& str, ssize_t index)
	: unicode_string(str), index(index)
	{
		UErrorCode err = U_ZERO_ERROR;
		iter = icu::BreakIterator::createCharacterInstance(Locale::getDefault(), err);
		if (!U_SUCCESS(err)) {
			throw u_errorName(err);
		}
		iter->setText(unicode_string);

		// jump to the right place in the string
		for (ssize_t i = 0; i < index && iter->next() != icu::BreakIterator::DONE; ++i) {
			// Do nothing here
		}
	}

	string::iterator::~iterator()
	{
		delete iter;
	}

	bool string::iterator::operator!=(const string::iterator& other)
	{
		return index != other.index;
	}

	string::iterator& string::iterator::operator++()
	{
		++index;
		iter->next();
		return *this;
	}

	character string::iterator::operator*()
	{
		icu::CharacterIterator *char_iterator = iter->getText().clone();
		icu::UnicodeString s;
		char_iterator->getText(s);
		delete char_iterator;
		return character(UnicodeString(s, index, 1));
	}

	//-----------------------------

	string to_string(int i)
	{
		char buf[100];
		sprintf(buf, "%d", i);
		return string(buf);
	}

	string to_string(unsigned long i)
	{
		char buf[100];
		sprintf(buf, "%lu", i);
		return string(buf);
	}
};
