#include "time_event.hh"
#include "event_base.hh"

#include <sys/time.h>

#include <iostream>
namespace eve
{

time_event::time_event(event_base *base)
    : event(base)
{
    std::cout << __func__ << std::endl;
    timerclear(&_timeout);
}

void time_event::set_timer(int nsec)
{
    struct timeval now, tv;
    gettimeofday(&now, NULL);

    timerclear(&tv);
    tv.tv_sec = nsec;

    timeradd(&now, &tv, &_timeout);
}

void time_event::add()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    this->_base->timeevset.insert(this);
}

void time_event::del()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    std::cout << "ERROR this function shouldn't be called\n";
}

} // namespace eve
