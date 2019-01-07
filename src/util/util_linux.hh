#pragma once

#include <unistd.h>

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
    std::cerr<<"[linux] close fd="<<fd<<std::endl;
    return close(fd);
}

std::string get_date();

} // namespace eve
