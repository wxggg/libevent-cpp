#pragma once

#include <unistd.h>
#include <sys/eventfd.h>

#include <string>
#include <iostream>

namespace eve
{

inline int closefd(int fd)
{
    if (fd <= 0)
    {
        std::cerr << "[linux] warning close fd=" << fd << std::endl;
        return -1;
    }
    // std::cout<<"[linux] close fd="<<fd<<std::endl;
    return close(fd);
}

inline int create_eventfd()
{
    int evfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evfd < 0)
    {
        std::cerr<<"[linux] eventfd error\n";
        abort();
    }
    return evfd;
}

std::string get_date();

} // namespace eve
