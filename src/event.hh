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
	event_base *base;
	short ncalls = 0;
	int pri; /* smaller numbers means higher priority */

	void (*callback)(void *arg);
	int res; /* result passed to event callback */

	short *ev_pncalls; /* allows deletes in callback */

  public:
	event(event_base *base);
	virtual ~event() { std::cout << __func__ << std::endl; }

	void set_base(event_base *base) { this->base = base; }
	void set_callback(void (*callback)(void *)) { this->callback = callback; }

	void set_persistent() { _persistent = true; }
	void clear_persistent() { _persistent = false; }
	bool is_persistent() { return _persistent; }

	virtual void add()=0;
	virtual void del()=0;

	void activate(short ncalls);
};

} // namespace eve
