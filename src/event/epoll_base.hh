#pragma once

#include "event_base.hh"

#include <sys/epoll.h>

namespace eve
{

class rw_event;
class epoll_base : public event_base
{
  private:
    struct epoll_event * _epevents;
    int _epfd;
    int _nfds;
  public:
    epoll_base();
    ~epoll_base();

    int add(rw_event *ev);
    int del(rw_event *ev);
    int dispatch(struct timeval *tv);
    int recalc();
};

} // namespace eve
