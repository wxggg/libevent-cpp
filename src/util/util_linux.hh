#pragma once

#include <unistd.h>
#include <sys/eventfd.h>

#include <string>
#include <iostream>

#include <logger.hh>

namespace eve
{

inline int closefd(int fd)
{
    if (fd <= 0)
    {
        LOG_WARN << "[linux] close fd=" << fd ;
        return -1;
    }
    LOG << "[FD] close fd=" << fd;
    return close(fd);
}

inline int create_eventfd()
{
    int evfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evfd < 0)
    {
        std::cerr << "[linux] eventfd error\n";
        abort();
    }
    LOG << " fd=" << evfd;
    return evfd;
}

void wake(int fd);
int read_wake_msg(int fd);

std::string get_date();

} // namespace eve
