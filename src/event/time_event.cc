#include <time_event.hh>
#include <event_base.hh>

#include <sys/time.h>

#include <iostream>
namespace eve
{

time_event::time_event(event_base *base)
    : event(base)
{
    timerclear(&timeout);
}

time_event::~time_event()
{
    del();
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

int time_event::add()
{
    this->base->timeevset.insert(this);
    return 0;
}

int time_event::del()
{
    this->base->timeevset.erase(this);
    return 0;
}

} // namespace eve
