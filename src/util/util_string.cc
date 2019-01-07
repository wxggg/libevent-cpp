#include <string>
#include <sstream>
#include <regex>

namespace eve
{

bool iequals(const std::string &a, const std::string &b)
{
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                      [](char a, char b) { return tolower(a) == tolower(b); });
}

bool iequals_n(const std::string &a, const std::string &b, int n)
{
    return std::equal(a.begin(), a.begin() + n, b.begin(), b.end(),
                      [](char a, char b) { return tolower(a) == tolower(b); });
}

bool is_palindrome(const std::string &s)
{
    return std::equal(s.begin(), s.begin() + s.size() / 2, s.rbegin());
}

std::string replace(const std::string &str, const std::string &from, const std::string &to)
{
    if (from.empty())
        return str;
    std::regex re("\\" + from);
    return std::regex_replace(str, re, to);
}

/*
 * Replaces <, >, ", ' and & with &lt;, &gt;, &quot;,
 * &#039; and &amp; correspondingly.
 */
void htmlescape(std::string &html)
{
    html = replace(html, "<", "&lt");
    html = replace(html, ">", "&gt");
    html = replace(html, "\"", "&quot");
    html = replace(html, "'", "&#039");
    html = replace(html, "&", "&amp");
}

std::vector<std::string> split(const std::string &s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream iss(s);
    while (std::getline(iss, token, delimiter))
        tokens.push_back(token);
    return tokens;
}

} // namespace eve
