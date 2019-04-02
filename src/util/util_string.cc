#include <string>
#include <sstream>
#include <regex>
#include <iostream>
#include <algorithm>

namespace eve
{

bool iequals(const std::string &a, const std::string &b)
{
    if (a.size() != b.size())
        return false;

    for (size_t i = 0; i < a.size(); i++) {
        if (::tolower(a[i]) != ::tolower(b[i]))
            return false;
    }

    return true;
}

bool iequals_n(const std::string &a, const std::string &b, int n)
{
    if (a.size() != b.size())
        return false;

    n = std::min(static_cast<int>(a.size()), n);

    for (size_t i = 0; i < static_cast<size_t>(n); i++) {
        if (::tolower(a[i]) != ::tolower(b[i]))
            return false;
    }

    return true;
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

int hex_to_int(char a)
{
    if (a >= '0' && a <= '9')
        return (a - 48);
    else if (a >= 'A' && a <= 'Z')
        return (a - 55);
    else
        return (a - 87);
}
std::string string_from_utf8(const std::string &in)
{
    std::string result;
    std::stringstream ss;
    size_t i = 0, n = in.length();
    bool flag = false;
    while (i < n)
    {
        char x = in[i++];
        if (x == '%')
        {
            flag = true;
            ss << static_cast<char>((16 * hex_to_int(in[i++]) + hex_to_int(in[i++])));
        }
        else
        {
            if (flag)
            {
                result += ss.str();
                std::stringstream().swap(ss);
            }
            result += x;
            flag = false;
        }
    }
    if (flag)
        result += ss.str();
    return result;
}

} // namespace eve
