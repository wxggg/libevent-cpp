#include "rw_event.hh"
#include "event_base.hh"

#include <iostream>
namespace eve
{

rw_event::rw_event(event_base *base)
    : event(base)
{
    std::cout << __func__ << std::endl;
}

void rw_event::add()
{
	std::cout << __PRETTY_FUNCTION__ << std::endl;
    this->_base->add(this);
}

void rw_event::del()
{
	std::cout << __PRETTY_FUNCTION__ << std::endl;
    this->_base->del(this);
}

} // namespace eve
