#pragma once

#include <istream>
#include <ostream>
#include <string>
#include <vector>

class History {
	public:
		History();

		void add(const std::string& entry);
		bool at_newest() const;
		bool changed() const;
		bool empty() const;
		const std::string& find(const std::string& s, ssize_t len);
		std::string newer();
		const std::string& newest() const;
		std::string older();

		friend std::istream& operator>> (std::istream& strean, History& history);
		friend std::ostream& operator<< (std::ostream& stream, const History& history);

	private:
		void add_raw(const std::string& entry);
		bool has_changed = false;

		std::vector<std::string> lines;
		ssize_t position, search_marker;
};
