#pragma once

#include <sstream>
#include <iostream>

namespace eve
{

class logger
{
  private:
    std::stringstream ss;

  public:
    logger();
    ~logger();

    template <typename T>
    logger &operator<<(T t)
    {
        ss << t;
        return *this;
    }
};

void init_log_file(const std::string &file);

#define LOG logger() << __func__ <<" " 
#define LOG_DEBUG LOG << "DEBUG: "
#define LOG_ERROR LOG << "ERROR: "
#define LOG_WARN LOG <<"WARN: "

} // namespace eve
