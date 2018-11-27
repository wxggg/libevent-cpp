#pragma once

#include <list>
#include <queue>
#include <vector>

#include <sys/time.h>

#define EVLIST_TIMEOUT 0x01
#define EVLIST_INSERTED 0x02
#define EVLIST_SIGNAL 0x04
#define EVLIST_ACTIVE 0x08
#define EVLIST_INTERNAL 0x10
#define EVLIST_INIT 0x80

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
	int ev_fd;
	short ev_events; /* EV_TIMEOUT EV_READ EV_WRITE EV_SIGNAL EV_PERSIST */
	short ev_ncalls;
	short *ev_pncalls; /* allows deletes in callback */

	int ev_pri; /* smaller numbers are higher priority */
	event_base *ev_base;
	void (*ev_callback)(int, short, void *arg);

	int ev_res; /* result passed to event callback */

	/** EVLIST_TIMEOUT EVLIST_INSERTED EVLIST_SIGNAL EVLIST_ACTIVE
	 * EVLIST_INTERNAL EVLIST_INIT EVLIST_ALL 
	 * EVLIST_INSERTED=READ/WRITE */
	int ev_flags;

	struct timeval ev_timeout;

  public:
	event() {}
	event(event_base *base);
	event(event_base *base, int fd, short events,
		  void (*callback)(int, short, void *));
	~event() {}

	void set_base(event_base *base);
	void set(event_base *base, int fd, short events, void (*callback)(int, short, void *));

	int dispatch(void);
	int loop(int flags);

	void active(int res, short ncalls);
};
class evsignal;
class event_base
{
  public:
	int event_count;		/* counts of total events */
	int event_count_active; /* counts of active events */
	int event_gotterm;		/* Set to terminate loop */

	
	std::vector<std::list<event *>> activequeues; 

	struct timeval event_tv;

	std::list<event *> eventqueue;
	std::list<event *> signalqueue;

	evsignal *evsig;

  public:
	event_base();
	virtual ~event_base() {}
	virtual int add(event *) { return 0; }
	virtual int del(event *) { return 0; }
	virtual int recalc(int max) { return max; }
	virtual int dispatch(struct timeval *) { return 0; }

	int priority_init(int npriorities);
	int loop(int flags);

	int add_event(event *ev, struct timeval *tv);
	int del_event(event *ev);
	
	void activate(event *ev, int res, short ncalls);

	void event_queue_remove(event *ev, int queue);
	void event_queue_insert(event *ev, int queue);

  private:
	void event_process_active();
};

} // namespace eve
