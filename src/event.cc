#include "event.hh"

#include <assert.h>
#include <iostream>
#include <iomanip>
#include <iterator>

namespace eve
{
/** class event ** 
 * 	core structure of libevent-cpp **/

event::event(event_base *base)
{
	std::cout << __func__ << std::endl;
	this->ev_base = base;
}

event::event(event_base *base, int fd, short events,
			 void (*callback)(int, short, void *))
	: ev_base(base), ev_callback(callback),
	  ev_fd(fd), ev_events(events),
	  ev_flags(EVLIST_INIT), ev_ncalls(0), ev_pncalls(NULL)
{
	std::cout << __func__ << std::endl;
	/* by default, we put new events into the middle priority */
	this->ev_pri = ev_base->activequeues.size() / 2;
}

void event::set(event_base *base, int fd, short events, void (*callback)(int, short, void *))
{
	std::cout << __func__ << " base=" << std::hex << base << std::endl;
	if (base)
	{
		this->ev_base = base;
	}

	if (!this->ev_base)
	{
		std::cout << "err ev_base not set yet\n";
	}

	this->ev_callback = callback;
	this->ev_fd = fd;
	this->ev_events = events;
	this->ev_flags = EVLIST_INIT;
	this->ev_ncalls = 0;
	this->ev_pncalls = NULL;
	/* by default, we put new events into the middle priority */
	this->ev_pri = ev_base->activequeues.size() / 2;
}

void event::set_base(event_base *base)
{
	std::cout << __func__ << std::endl;
	this->ev_base = base;
}

/** class event_base ** 
 * 	to manage event queues **/

event_base::event_base()
	: event_count(0), event_count_active(0), event_gotterm(0)
{
	std::cout << __func__ << std::endl;
	gettimeofday(&event_tv, NULL);
}

int event_base::priority_init(int npriorities)
{
	std::cout << __func__ << std::endl;
	if (this->event_count_active)
		return -1;

	if (this->activequeues.size() && npriorities != activequeues.size())
	{
		for (auto item : activequeues)
		{
			item.clear();
		}
		activequeues.clear();
	}

	/* allocate priority queues */
	for (int i = 0; i < npriorities; i++)
	{
		std::list<event *> item; //Todo: figure out ??
		activequeues.push_back(item);
	}

	std::cout << __func__ << " activequeues.size()=" << activequeues.size() << std::endl;

	return 0;
}

int event_base::loop(int flags)
{
	std::cout << __func__ << std::endl;
	/* Calculate the initial events that we are waiting for */
	if (this->recalc(0) == -1)
		return -1;

	int done = 0;
	struct timeval tv;
	int i = 0;
	while (!done && i++ < 5)
	{
		std::cout << "i=" << i << std::endl;
		/* Terminate the loop if we have been asked to */
		if (this->event_gotterm)
		{
			std::cout << "event got terminated\n";
			this->event_gotterm = 0;
			break;
		}

		/* Check if time is running backwards */
		gettimeofday(&tv, NULL);
		if (timercmp(&tv, &this->event_tv, <))
		{
			struct timeval off;
			timersub(&this->event_tv, &tv, &off);
			// timeout_correct()
		}
		this->event_tv = tv;

		if (!this->event_count_active && !(flags & EVLOOP_NONBLOCK))
			; // Todo: timeout_next
		else
			timerclear(&tv);

		/* If we have no events, we just exit */
		if (this->event_count < 1)
		{
			std::cout << "have no events, just exit\n";
			return 1;
		}

		if (this->dispatch(&tv) == -1)
		{
			std::cout << "dispatch exit -1\n";
			return -1;
		}

		if (this->event_count_active)
		{
			event_process_active();
			std::cout << "---- event process active finished -----\n";
			if (!this->event_count_active && (flags & EVLOOP_ONCE))
				done = 1;
		}
		else if (flags & EVLOOP_NONBLOCK)
			done = 1;

		if (this->recalc(0) == -1)
		{
			std::cout << "recalc exited -1\n";
			return -1;
		}
	}
	std::cout << "exit with done=" << done << std::endl;
	return 0;
}

int event_base::add_event(event *ev, struct timeval *tv)
{
	std::cout << __func__ << " ev->fd=" << ev->ev_fd << std::endl;
	assert(!(ev->ev_flags & ~EVLIST_ALL));

	if (tv != NULL)
	{
		if (ev->ev_flags & EVLIST_TIMEOUT)
			event_queue_remove(ev, EVLIST_TIMEOUT);

		/* Check if it is active due to a timeout.  Rescheduling
		 * this timeout before the callback can be executed
		 * removes it from the active list. */
		if ((ev->ev_flags & EVLIST_ACTIVE))
		{ // todo :: ?
			// todo :: ??
			event_queue_remove(ev, EVLIST_ACTIVE);
		}

		struct timeval now;
		gettimeofday(&now, NULL);
		timeradd(&now, tv, &ev->ev_timeout);

		event_queue_insert(ev, EVLIST_TIMEOUT);
	}

	if ((ev->ev_events & (EV_READ | EV_WRITE)) &&
		!(ev->ev_flags & (EVLIST_INSERTED | EVLIST_ACTIVE)))
	{
		event_queue_insert(ev, EVLIST_INSERTED);
		return this->add(ev);
	}
	else if ((ev->ev_events & EV_SIGNAL) &&
			 !(ev->ev_flags & EVLIST_SIGNAL))
	{
		event_queue_insert(ev, EVLIST_SIGNAL);
		return this->add(ev);
	}

	return 0;
}

int event_base::del_event(event *ev)
{
	std::cout << __func__ << std::endl;
	assert(!(ev->ev_flags & ~EVLIST_ALL));

	if (ev->ev_flags & EVLIST_TIMEOUT)
		event_queue_remove(ev, EVLIST_TIMEOUT);

	if (ev->ev_flags & EVLIST_ACTIVE)
		event_queue_remove(ev, EVLIST_ACTIVE);

	if (ev->ev_flags & EVLIST_INSERTED)
	{
		event_queue_remove(ev, EVLIST_INSERTED);
		return this->del(ev);
	}
	else if (ev->ev_flags & EVLIST_SIGNAL)
	{
		event_queue_remove(ev, EVLIST_SIGNAL);
		return this->del(ev);
	}

	return 0;
}

void event_base::activate(event *ev, int res, short ncalls)
{
	std::cout << __func__ << std::endl;
	/* We get different kinds of events, add them together */
	if (ev->ev_flags & EVLIST_ACTIVE)
	{
		ev->ev_res |= res;
		return;
	}
	
	ev->ev_flags |= EVLIST_ACTIVE;
	ev->ev_res = res;
	ev->ev_ncalls = ncalls;
	// pncalls ??

	this->event_count_active++;
	this->activequeues[ev->ev_pri].push_back(ev);
}

void event_base::event_queue_remove(event *ev, int queue)
{
	std::cout << __func__ << " queue=0x" << std::hex << queue << std::endl;
	if (!(ev->ev_flags & queue))
	{
		std::cerr << __func__ << " " << ev->ev_fd << " not on queue " << queue << std::endl;
		exit(-1);
	}

	int docount = 1;

	if (ev->ev_flags & EVLIST_INTERNAL)
		docount = 0;

	if (docount)
		this->event_count--;

	ev->ev_flags &= ~queue;

	switch (queue)
	{
	case EVLIST_SIGNAL:
		this->signalqueue.remove(ev);
		break;
	case EVLIST_TIMEOUT:
		break;
	case EVLIST_INSERTED:
		this->eventqueue.remove(ev);
		break;
	default:
		break;
	}
}

void event_base::event_queue_insert(event *ev, int queue)
{
	std::cout << __func__ << " queue=0x" << std::hex << queue << std::endl;
	if (ev->ev_flags & queue)
	{
		/* Double insertion is possible for active events */
		if (queue & EVLIST_ACTIVE)
			return;
		std::cout << "[ERR] " << __func__ << std::endl;
	}

	int docount = 1;
	if (ev->ev_flags & EVLIST_INTERNAL)
		docount = 0;

	if (docount)
		this->event_count++;

	ev->ev_flags |= queue;
	switch (queue)
	{
	case EVLIST_SIGNAL:
		this->signalqueue.push_back(ev);
		break;
	case EVLIST_TIMEOUT:
		break;
	case EVLIST_INSERTED:
		this->eventqueue.push_back(ev);
		break;
	default:
		std::cout << "error: unknow queue\n";
		break;
	}
}

void event_base::event_process_active()
{
	std::cout << __func__ << std::endl;

	if (!this->event_count_active || this->activequeues.empty())
		return;

	std::list<event *> &activeq = this->activequeues[0];
	for (auto &item : this->activequeues)
	{
		if (!item.empty())
		{
			activeq = item;
			break;
		}
	}

	event *ev;
	std::list<event *>::iterator i = activeq.begin();
	while (i != activeq.end())
	{
		ev = *i;
		while(ev->ev_ncalls) {
			ev->ev_ncalls--;
			(*ev->ev_callback)((int)ev->ev_fd, ev->ev_res, ev);
		}
		this->event_count--;
		this->event_count_active--;
		i = activeq.erase(i);
	}

}

} // namespace eve
