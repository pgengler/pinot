#include "History.h"

#include "pinot.h"

History::History()
{
	position = search_marker = 0;
}

void History::add(const string& entry)
{
	DEBUG_LOG("Adding '" << entry << "' to history");
	// If the list already includes this entry, remove it
	for (auto iter = lines.begin(); iter != lines.end(); ++iter) {
		if (*iter == entry) {
			lines.erase(iter);
			break;
		}
	}

	// If adding this would put us over the max number of entries to save, remove the oldest one
	if (lines.size() + 1 > MAX_SEARCH_HISTORY) {
		lines.pop_back();
	}

	// Add this entry
	lines.insert(lines.begin(), entry);

	has_changed = true;
}

void History::add_raw(const string& entry)
{
	lines.push_back(entry);
}

bool History::at_newest() const
{
	return position == 0;
}

bool History::changed() const
{
	return has_changed;
}

bool History::empty() const
{
	return lines.empty();
}

const string& History::find(const string& s, ssize_t len)
{
	if (s == "" || empty()) {
		return s;
	}

	for (auto i = search_marker; i < lines.size(); ++i) {
		DEBUG_LOG("Comparing '" << lines[i] << "' with '" << s << "' (" << len << " chars)");
		if (lines[i].compare(0, len, s) == 0) {
			search_marker = i;
			return lines[i];
		}
	}

	for (auto i = 0; i < search_marker; ++i) {
		DEBUG_LOG("Comparing '" << lines[i] << "' with '" << s << "' (" << len << " chars)");
		if (lines[i].compare(0, len, s) == 0) {
			search_marker = i;
			return lines[i];
		}
	}

	return s;
}

string History::newer()
{
	if (position == 0) {
		return "";
	}
	return lines[--position];
}

const string& History::newest() const
{
	return lines.front();
}

string History::older()
{
	if (position == lines.size()) {
		return "";
	}
	return lines[++position];
}

std::istream& operator>> (std::istream& stream, History& history)
{
	string line;
	while (stream) {
		getline(stream, line);
		if (line != "") {
			history.add_raw(line);
		} else {
			break;
		}
	}

	return stream;
}

std::ostream& operator<< (std::ostream& stream, const History& history)
{
	for (auto line : history.lines) {
		stream << line << std::endl;
	}
	stream << std::endl;

	return stream;
}
