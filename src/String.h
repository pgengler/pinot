#pragma once

#include <cstring>
#include <istream>
#include <memory>
#include <ostream>
#include <string>

#include <unicode/unistr.h>
#include <unicode/ustream.h>
#include <unicode/brkiter.h>

namespace pinot {
	class string;

	class character
	{
	public:
		character(char ch);
		character(const icu::UnicodeString& s);

		bool operator==(char ch);
		bool operator!=(char ch);

		bool is_blank() const;

		friend class string;
		friend std::ostream& operator<<(std::ostream& stream, const character& ch);
	private:
		icu::UnicodeString str;
	};

	class string
	{
	public:
		string();
		string(char ch);
		string(const character& ch);
		string(const char *str);
		string(const icu::UnicodeString& str);
		string(const std::string& str);
		string(const string& str);

		virtual ~string();

		string& append(const char *str, size_t chars);
		string& remove(size_t pos, size_t len=1);
		string& replace(size_t pos, char c);

		bool compare(size_t pos, size_t len, const string& str) const;
		const char *c_str();
		bool empty() const;
		ssize_t length() const;
		bool starts_with(const string& str) const;
		std::string str() const;

		character front() const;
		character back() const;

		string substr(size_t pos=0, size_t len=npos) const;

		string to_lower() const;

		ssize_t find(const string& other) const;
		ssize_t rfind(const string& other) const;

		ssize_t find_first_of(const string& other) const;

		character operator[](size_t index) const;

		string& operator+=(const string& other);

		bool operator==(const string& other) const;
		bool operator==(const char *other) const;

		bool operator!=(const string& other) const;
		bool operator!=(const char *other) const;

		bool operator<(const string& other) const;
		bool operator<=(const string& other) const;
		bool operator>(const string& other) const;
		bool operator>=(const string& other) const;

		int32_t hash_code() const;

		class iterator {
		public:
			iterator(const icu::UnicodeString& str, ssize_t index);
			virtual ~iterator();

			bool operator!= (const iterator& other);
			iterator& operator++();
			character operator*();

		private:
			const icu::UnicodeString& unicode_string;
			icu::BreakIterator* iter;
			ssize_t index;
		};

		string::iterator begin() const;
		string::iterator end() const;

		friend std::istream& getline(std::istream& stream, string& str);
		friend std::istream& operator>>(std::istream& stream, string& str);
		friend std::ostream& operator<<(std::ostream& stream, const string& str);

		friend string operator+(string a, const string& b);

		static const ssize_t npos = static_cast<ssize_t>(-1);
	private:
		icu::UnicodeString unicode_str;
		char* c_str_buffer = nullptr;
	};

	string to_string(int i);
	string to_string(unsigned long i);
};

namespace std {
	template<>
	struct hash<pinot::string>
	{
		int32_t operator()(pinot::string const& s) const
		{
			return s.hash_code();
		}
	};
}
