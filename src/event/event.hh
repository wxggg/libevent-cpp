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

#include <logger.hh>

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
	std::weak_ptr<event_base> base;
	short ncalls = 0;
	int pri; /* smaller numbers means higher priority */

	int err = -1;

	bool alive = false;

  public:
	event() {}
	event(std::shared_ptr<event_base> base);
	virtual ~event() {}

	virtual void init(std::shared_ptr<event>) {}

	void set_base(std::shared_ptr<event_base> base);

	decltype(auto) get_base()
	{
		auto b = base.lock();
		if (!b)
			LOG_ERROR << "error base is expired\n";
		return b;
	}

	inline void set_active() { _active = true; }
	inline void clear_active() { _active = false; }
	inline bool is_active() { return _active; }

	inline void set_persistent() { _persistent = true; }
	inline void clear_persistent() { _persistent = false; }
	inline bool is_persistent() const { return _persistent; }

	void set_priority(int pri);
};

template <typename T, typename... Rest>
decltype(auto) create_event(Rest &&... rest)
{
	auto ev = std::make_shared<T>(std::forward<Rest>(rest)...);
	ev->init(ev);
	return ev;
}

} // namespace eve
