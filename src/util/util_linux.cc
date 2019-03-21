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
    time_t t = time(nullptr);
    gmtime_r(&t, &cur);
    cur_p = &cur;
    if (strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", cur_p) != 0)
        return std::string(date);
    return "";
}

void wake(int fd)
{
    std::string msg = "0x123456";
    size_t n = write(fd, msg.c_str(), msg.length());
    if (n <= 0)
        LOG_WARN << " wake write error\n";
}

int read_wake_msg(int fd)
{
    char buf[32];
    if (read(fd, buf, sizeof(buf)) <= 0)
    {
        LOG_WARN << " read error\n";
        return -1;
    }
    return 0;
}

} // namespace eve
