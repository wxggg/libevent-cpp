#pragma once

#include <list>
#include <queue>
#include <vector>
#include <iostream>
#include <functional>
#include <memory>
#include <future>

#include <sys/time.h>
#include <signal.h>

namespace eve
{
class event;
class event_base;

using Callback = std::function<void()>;

enum ERR
{
	E_EOF = -127,
	E_TIMEOUT = -126,
	E_UNKNOW,
};

class event
{
  private:
	static int _internal_event_id;
	bool _persistent = false;
	bool _active = false;

  public:
	int id;
	std::shared_ptr<event_base> base;
	short ncalls = 0;
	int pri; /* smaller numbers means higher priority */

	// EvCallback callback;
	// int res; /* result passed to event callback */

	// Callback *pcb;
	std::unique_ptr<Callback> pcb;

	short *ev_pncalls; /* allows deletes in callback */

	void *data; /* can be used to store anything */

	std::shared_ptr<void> ptr; /* used to store smart pointer */

	int err = -1;

  public:
	event() {}
	event(std::shared_ptr<event_base> base);
	virtual ~event() {}

	void set_base(std::shared_ptr<event_base> base);


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
		std::cerr << "warning: event id=" << ev->id << " called default callback\n";
	}

	template <typename F, typename... Rest>
	decltype(auto) set_callback(F &&f, Rest &&... rest)
	{
		auto tsk = std::bind(std::forward<F>(f), std::forward<Rest>(rest)...);
		this->pcb = std::make_unique<Callback>([tsk]() { tsk(); });
	}
};

} // namespace eve
