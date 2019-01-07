#include <util_linux.hh>

#include <time.h>
#include <string>

namespace eve
{
    
std::string get_date()
{
    char date[50];
    struct tm cur;
    struct tm *cur_p;
    time_t t = time(NULL);
    gmtime_r(&t, &cur);
    cur_p = &cur;
    if (strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", cur_p) != 0)
        return std::string(date);
    return "";
}


} // nameeve
