#pragma once
#include <algorithm> 
#include <cctype>
#include <locale>
#include <vector>

// trim from start (in place)
static inline void ltrim(std::string &s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
		return !std::isspace(ch);
	}));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
		return !std::isspace(ch);
	}).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
	ltrim(s);
	rtrim(s);
}

// trim from start (copying)
static inline std::string ltrim_copy(std::string s) {
	ltrim(s);
	return s;
}

// trim from end (copying)
static inline std::string rtrim_copy(std::string s) {
	rtrim(s);
	return s;
}

// trim from both ends (copying)
static inline std::string trim_copy(std::string s) {
	trim(s);
	return s;
}

static inline bool startsWith(const std::string& s, const std::string& start)
{
	return s.substr(0, start.size()) == start;
}

static inline std::vector<std::string> split(const std::string& s, const std::string& search, bool remove_empty = true)
{
	std::vector<std::string> res;
	auto pos = s.find(search);
	size_t offset = 0;
	while (pos != std::string::npos) {
		auto elem = s.substr(offset, pos - offset);
		if (!elem.empty() || !remove_empty)
			res.push_back(elem);
		offset = pos + search.size();
		pos = s.find(search, offset);
	};

	std::string last = s.substr(offset, pos - offset);
	if (!last.empty() || !remove_empty)
		res.push_back(last);
	return res;
}

static inline std::string join(const std::string& delim, const std::vector<std::string>& parts, size_t offset = 0, size_t len = size_t(-1))
{
	std::string res;
	if (len == size_t(-1))
		len = parts.size();
	auto max = std::min(parts.size(), offset + len);
	for (size_t i = offset; i < max; i++) {
		res += parts[i];
		if (i != parts.size() - 1)
			res += delim;
	}
	return res;
}