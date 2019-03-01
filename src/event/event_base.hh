#pragma once

#include <signal.h>

#include <vector>
#include <list>
#include <set>
#include <map>
#include <iostream>
#include <utility>

namespace eve
{

class event;
class rw_event;
class signal_event;
class time_event;
struct cmp_timeev;

struct cmp_timeev
{
	bool operator()(time_event *const &lhs, time_event *const &rhs) const;
};

class event_base
{
  private:
	bool _loop_nonblock = false;
	bool _loop_once = false;
	bool _terminated = false;
	int i = 0;

  public:
	void set_terminated(bool flag) { _terminated = flag; }

	std::vector<std::list<event *>> activequeues;

	std::list<event *> eventqueue;
	std::list<signal_event *> signalqueue;
	std::set<time_event *, cmp_timeev> timeevset;

	sigset_t evsigmask;
	rw_event *readsig;

	static std::vector<int> sigcaught;
	static volatile sig_atomic_t caught;
	static std::pair<int, int> signal_pair;
	static int needrecalc;

	int _fds = 0; /* highest fd of added rw_event */
	int _fdsz = 0;
	std::map<int, rw_event *> fd_map_rw;

  public:
	event_base();
	virtual ~event_base(){};
	virtual int add(rw_event *) { return 0; }
	virtual int del(rw_event *) { return 0; }
	virtual int recalc() = 0;
	virtual int dispatch(struct timeval *) { return 0; }

	int count_rw_events() { return fd_map_rw.size(); }

	int priority_init(int npriorities);

	int loop();
	inline void loop_nonblock_and_once()
	{
		set_loop_nonblock();
		loop();
		clear_loop_flags();
	}

	void event_process_active();
	void timeout_process();

	inline void set_loop_nonblock() { _loop_nonblock = true; }
	inline void clear_loop_nonblock() { _loop_nonblock = false; }

	inline void set_loop_once() { _loop_once = true; }
	inline void clear_loop_once() { _loop_once = true; }

	inline void set_loop_nonblock_and_once() { _loop_nonblock = _loop_once = true; }
	inline void clear_loop_flags() { _loop_nonblock = _loop_once = false; }

	int count_active_events()
	{
		int ret = 0;
		for (const auto &aq : activequeues)
			ret += aq.size();
		return ret;
	}

  protected:
	void evsignal_process();
	int evsignal_recalc();
	int evsignal_deliver();

  private:
	static void handler(int sig);
	static void readsig_cb();
};

} // namespace eve
