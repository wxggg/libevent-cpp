#pragma once

#include <signal.h>
#include <sys/time.h>

#include <vector>
#include <list>
#include <set>
#include <queue>
#include <map>
#include <iostream>
#include <utility>
#include <functional>
#include <memory>

namespace eve
{
using Callback = std::function<void()>;

class event;
class rw_event;
class signal_event;
class time_event;
struct cmp_timeev;

struct cmp_timeev
{
	bool operator()(std::shared_ptr<time_event> const &lhs, std::shared_ptr<time_event> const &rhs) const;
};

class event_base
{
  private:
	bool _loop_nonblock = false;
	bool _loop_once = false;
	bool _terminated = false;
	int i = 0;

	std::vector<std::queue<std::shared_ptr<event>>> activeQueues;
	std::list<std::shared_ptr<signal_event>> signalList;
	std::set<std::shared_ptr<time_event>, cmp_timeev> timeSet;
	std::map<int, std::shared_ptr<Callback>> callbackMap;

  protected:
	std::map<int, std::shared_ptr<rw_event>> fdMapRw;

  public:
	sigset_t evsigmask;

	static std::vector<int> sigcaught;
	static volatile sig_atomic_t caught;
	static std::pair<int, int> signal_pair;
	static int needrecalc;

	int _fds = 0; /* highest fd of added rw_event */
	int _fdsz = 0;

  public:
	event_base();
	virtual ~event_base()
	{
		__clean_up();
	}

	virtual int add(std::shared_ptr<rw_event>) { return 0; }
	virtual int del(std::shared_ptr<rw_event>) { return 0; }
	virtual int recalc() = 0;
	virtual int dispatch(struct timeval *) { return 0; }

	inline int active_queue_size() { return static_cast<int>(activeQueues.size()); }
	inline int active_event_size()
	{
		int ret = 0;
		for (const auto &aq : activeQueues)
			ret += aq.size();
		return ret;
	}
	inline int rw_event_size() { return fdMapRw.size(); }

	int priority_init(int npriorities);

	int add_event(const std::shared_ptr<event> &ev);
	int remove_event(const std::shared_ptr<event> &ev);
	void clean_event(const std::shared_ptr<event> &ev);

	template <typename E, typename F, typename... Rest>
	decltype(auto) register_callback(E &&e, F &&f, Rest &&... rest)
	{
		auto tsk = std::bind(std::forward<F>(f), std::forward<Rest>(rest)...);
		callbackMap[e->id] = std::make_shared<Callback>([tsk]() { tsk(); });
	}

	void activate(std::shared_ptr<event> ev, short ncalls);
	void activate_read(std::shared_ptr<rw_event> ev);
	void activate_write(std::shared_ptr<rw_event> ev);

	int loop();

	void process_active_events();
	void process_timeout_events();

	inline void set_terminated() { _terminated = true; }

	inline void loop_nonblock_and_once()
	{
		set_loop_nonblock_and_once();
		__loop();
		clear_loop_flags();
	}

	inline void set_loop_nonblock() { _loop_nonblock = true; }
	inline void clear_loop_nonblock() { _loop_nonblock = false; }

	inline void set_loop_once() { _loop_once = true; }
	inline void clear_loop_once() { _loop_once = true; }

	inline void set_loop_nonblock_and_once() { _loop_nonblock = _loop_once = true; }
	inline void clear_loop_flags() { _loop_nonblock = _loop_once = false; }

  protected:
	void evsignal_process();
	int evsignal_recalc();
	int evsignal_deliver();

  private:
	static void handler(int sig);
	int __loop();
	void __clean_up();
};

} // namespace eve
