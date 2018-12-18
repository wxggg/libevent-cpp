#include "time_event.hh"
#include "event_base.hh"

#include <sys/time.h>

#include <iostream>
namespace eve
{

time_event::time_event(event_base *base)
    : event(base)
{
    timerclear(&timeout);
}

void time_event::set_timer(int sec, int usec)
{
    struct timeval now, tv;
    gettimeofday(&now, NULL);

    timerclear(&tv);
    tv.tv_sec = sec;
    tv.tv_usec = usec;

    timeradd(&now, &tv, &timeout);
}

void time_event::add()
{
    this->base->timeevset.insert(this);
}

void time_event::del()
{
    this->base->timeevset.erase(this);
}

} // namespace eve
