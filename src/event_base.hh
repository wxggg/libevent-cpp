#pragma once

#include <signal.h>
#include <sys/time.h>

#include <vector>
#include <list>
#include <set>
#include <map>
#include <iostream>

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
  public:
	int event_gotterm = 0; /* Set to terminate loop */

	std::vector<std::list<event *>> activequeues;

	std::list<event *> eventqueue;
	std::list<signal_event *> signalqueue;
	std::set<time_event *, cmp_timeev> timeevset;

	sigset_t evsigmask;
	rw_event *readsig;

	static short evsigcaught[NSIG];
	static volatile sig_atomic_t caught;
	static int ev_signal_pair[2];
	static int needrecalc;

	std::map<int, rw_event *> fd_map_rw;
	int _fds = 0; /* highest fd of added rw_event */
	int _fdsz = 0;

  public:
	event_base();
	virtual ~event_base(){};
	virtual int add(rw_event *) { return 0; }
	virtual int del(rw_event *) { return 0; }
	virtual int recalc(int max) { return 0; }
	virtual int dispatch(struct timeval *) { return 0; }

	int count_rw_events() { return fd_map_rw.size(); }

	int priority_init(int npriorities);
	int loop(int flags);

	void event_process_active();
	void timeout_process();

  protected:
	void evsignal_process();
	int evsignal_recalc();
	int evsignal_deliver();

  private:
	static void handler(int sig);
	static void readsig_cb(void *arg);
};

} // namespace eve
