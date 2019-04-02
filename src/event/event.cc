#include <event.hh>
#include <event_base.hh>

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
	set_base(base);
	id = _internal_event_id++;
}

void event::init(std::shared_ptr<event> const &e)
{
	(void)e;
}

void event::set_base(std::shared_ptr<event_base> base)
{
	this->base = base;
	this->pri = base->active_queue_size() / 2;
}

std::shared_ptr<event_base> event::get_base()
{
	std::shared_ptr<event_base> b = base.lock();
	if (!b)
		LOG_ERROR << "error base is expired\n";
	return b;
}


void event::set_priority(int pri)
{
	if (is_active())
		return;
	if (pri < 0 || pri >= get_base()->active_queue_size())
		return;
	this->pri = pri;
}

} // namespace eve
