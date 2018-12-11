#include "event.hh"
#include "event_base.hh"

#include <assert.h>
#include <iostream>
#include <iomanip>
#include <iterator>

#include <sys/socket.h>

namespace eve
{
/** class event ** 
 * 	core structure of libevent-cpp **/

event::event(event_base *base)
{
	std::cout << __func__ << std::endl;
	this->base = base;
	this->pri = this->base->activequeues.size() / 2;
}

void event::activate(short ncalls)
{
    std::cout << __func__ << std::endl;

	this->ncalls = ncalls;
	this->base->activequeues[this->pri].push_back(this);
}





} // namespace eve
