#include <string>
#include <vector>
#include <algorithm>

namespace eve
{
static const char *ws = " \t\n\r\f\v";

// trim from end of string (right)
inline std::string &rtrim(std::string &s, const char *t = ws)
{
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

// trim from beginning of string (left)
inline std::string &ltrim(std::string &s, const char *t = ws)
{
    s.erase(0, s.find_first_not_of(t));
    return s;
}

// trim from both ends of string (left & right)
inline std::string &trim(std::string &s, const char *t = ws)
{
    return ltrim(rtrim(s, t), t);
}

bool iequals(const std::string &a, const std::string &b);
bool iequals_n(const std::string &a, const std::string &b, int n);

std::string replace(const std::string &str, const std::string &from, const std::string &to);

void htmlescape(std::string &html);

std::vector<std::string> split(const std::string &s, char delimiter);

} // namespace eve
