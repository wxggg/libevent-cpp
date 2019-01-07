#pragma once

#include <list>
#include <queue>
#include <vector>
#include <iostream>

#include <sys/time.h>
#include <signal.h>

namespace eve
{

enum ERR {
	E_EOF = -127,
	E_TIMEOUT = -126,
	E_UNKNOW,
};

class event_base;
class event
{
private:
	static int _internal_event_id;
	bool _persistent = false;
	bool _active = false;

public:
	int id;
	event_base *base;
	short ncalls = 0;
	int pri; /* smaller numbers means higher priority */

	void (*callback)(event *ev);
	int res; /* result passed to event callback */

	short *ev_pncalls; /* allows deletes in callback */

	void *data; /* can be used to store anything */

	int err = -1;

public:
	event(event_base *base);
	virtual ~event() {}

	inline void set_base(event_base *base) { this->base = base; }
	inline void set_callback(void (*callback)(event *)) { this->callback = callback; }

	inline void set_active() { _active = true; }
	inline void clear_active() { _active = false; }
	inline bool is_active() { return _active; }

	inline void set_persistent() { _persistent = true; }
	inline void clear_persistent() { _persistent = false; }
	inline bool is_persistent() const { return _persistent; }

	virtual int add() = 0;
	virtual int del() = 0;

	void set_priority(int pri);
	void activate(short ncalls);


	static void default_callback(event *ev)
	{
		std::cerr<<"warning: event id="<<ev->id<<" called default callback\n";
	}
};

} // namespace eve
