#include "rw_event.hh"
#include "event_base.hh"

#include <iostream>
namespace eve
{

rw_event::rw_event(event_base *base)
    : event(base)
{
    std::cout << __func__ << std::endl;
    this->_events = 0;
}

void rw_event::add()
{
	std::cout << __PRETTY_FUNCTION__ << std::endl;
    this->_base->eventqueue.push_back(this);
}

void rw_event::del()
{
	std::cout << __PRETTY_FUNCTION__ << std::endl;
    this->_base->eventqueue.remove(this);
}

} // namespace eve
