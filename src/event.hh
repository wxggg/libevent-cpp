#pragma once

#include <list>
#include <queue>
#include <vector>
#include <iostream>

#include <sys/time.h>
#include <signal.h>


/* EVLIST_X_ Private space: 0x1000-0xf000 */
#define EVLIST_ALL (0xf000 | 0x9f)

#define EV_TIMEOUT 0x01
#define EV_READ 0x02
#define EV_WRITE 0x04
#define EV_SIGNAL 0x08
#define EV_PERSIST 0x10 /* Persistant event */

#define EVLOOP_ONCE 0x01
#define EVLOOP_NONBLOCK 0x02

namespace eve
{

class event_base;
class event
{
  public:
	event_base *_base;
	short _events; /* EV_TIMEOUT EV_READ EV_WRITE EV_SIGNAL EV_PERSIST */
	short _ncalls = 0;
	int _pri; /* smaller numbers means higher priority */

	void (*_callback)(short, void *arg);
	int _res; /* result passed to event callback */

	short *ev_pncalls; /* allows deletes in callback */

  public:
	event(event_base *base);
	virtual ~event() { std::cout << __func__ << std::endl; }

	void set_base(event_base *base) { _base = base; }
	void set_callback(void (*callback)(short, void *)) { _callback = callback; }

	virtual void add() {}
	virtual void del() {}

	void activate(int res, short ncalls);
};

} // namespace eve
