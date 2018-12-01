#pragma once

#include <list>
#include <queue>
#include <vector>
#include <iostream>

#include <sys/time.h>
#include <signal.h>

namespace eve
{

class event_base;
class event
{
  private:
	bool _persistent = false;
  public:
	event_base *_base;
	short _ncalls = 0;
	int _pri; /* smaller numbers means higher priority */

	void (*_callback)(void *arg);
	int _res; /* result passed to event callback */

	short *ev_pncalls; /* allows deletes in callback */

  public:
	event(event_base *base);
	virtual ~event() { std::cout << __func__ << std::endl; }

	void set_base(event_base *base) { _base = base; }
	void set_callback(void (*callback)(void *)) { _callback = callback; }

	void set_persistent() { _persistent = true; }
	void clear_persistent() { _persistent = false; }
	bool is_persistent() { return _persistent; }

	virtual void add() {}
	virtual void del() {}

	void activate(short ncalls);
};

} // namespace eve
