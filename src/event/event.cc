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

int event::_internal_event_id = 0;

event::event(std::shared_ptr<event_base> base)
{
	this->base = base;
	this->pri = this->base->activequeues.size() / 2;
	id = _internal_event_id++;

	set_callback(default_callback, this);
}

void event::set_priority(int pri)
{
	if (is_active())
		return;
	if (pri < 0 || pri >= static_cast<int>(this->base->activequeues.size()))
		return;
	this->pri = pri;
}

void event::activate(short ncalls)
{
	this->ncalls = ncalls;
	this->base->activequeues[this->pri].push_back(this);
	set_active();
}

} // namespace eve
