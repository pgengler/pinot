#pragma once

#include <istream>
#include <ostream>
#include <vector>

#include "String.h"
using pinot::string;

class History {
	public:
		History();

		void add(const string& entry);
		bool at_newest() const;
		bool changed() const;
		bool empty() const;
		const string& find(const string& s, ssize_t len);
		string newer();
		const string& newest() const;
		string older();

		friend std::istream& operator>> (std::istream& stream, History& history);
		friend std::ostream& operator<< (std::ostream& stream, const History& history);

	private:
		void add_raw(const string& entry);
		bool has_changed = false;

		std::vector<string> lines;
		ssize_t position, search_marker;
};
