#include "event_base.hh"
#include "rw_event.hh"
#include "signal_event.hh"
#include "time_event.hh"

#include <sys/socket.h>
#include <string.h>
#include <unistd.h>

#include <iostream>

namespace eve
{

short event_base::evsigcaught[NSIG];
volatile sig_atomic_t event_base::caught = 0;
int event_base::ev_signal_pair[2];
int event_base::needrecalc = 0;

bool cmp_timeev::operator()(time_event *const &lhs, time_event *const &rhs) const
{
	return timercmp(&lhs->_timeout, &rhs->_timeout, <);
}

event_base::event_base()
{
	std::cout << __func__ << std::endl;

	sigemptyset(&evsigmask);

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, ev_signal_pair) == -1)
		std::cout << __func__ << " socketpair error\n";

	readsig = new rw_event(this);
	readsig->set_read();
	readsig->set_callback(readsig_cb);
	readsig->set_fd(ev_signal_pair[1]);
	readsig->add();
}

int event_base::priority_init(int npriorities)
{
	std::cout << __func__ << std::endl;

	if (this->activequeues.size() && npriorities != activequeues.size())
	{
		for (auto item : activequeues)
		{
			item.clear();
		}
		activequeues.clear();
	}

	/* allocate priority queues */
	for (int i = 0; i < npriorities; i++)
	{
		std::list<event *> item; //Todo: figure out ??
		activequeues.push_back(item);
	}

	std::cout << __func__ << " activequeues.size()=" << activequeues.size() << std::endl;

	return 0;
}

int event_base::loop(int flags)
{
	std::cout << __PRETTY_FUNCTION__ << std::endl;
	/* Calculate the initial events that we are waiting for */
	if (this->recalc(0) == -1)
		return -1;

	int done = 0;
	struct timeval tv;
	int i = 0;
	while (!done && i++ < 5)
	{
		std::cout << __PRETTY_FUNCTION__ << " i=" << i << std::endl;
		/* Terminate the loop if we have been asked to */
		if (this->event_gotterm)
		{
			std::cout << "event got terminated\n";
			this->event_gotterm = 0;
			break;
		}

		bool hasactive = false;
		for (const auto &aq : activequeues)
		{
			if (!aq.empty())
				hasactive = true;
		}

		/* If we have no events, we just exit */
		if (signalqueue.empty() && timeevset.empty() && !count_rw_events() && !hasactive)
		{
			std::cout << "have no events, just exit\n";
			return 1;
		}

		int res;
		struct timeval now, off;
		gettimeofday(&now, NULL);
		if (timeevset.empty())
		{
			/* no timeout event */
			res = this->dispatch(NULL);
		}
		else
		{
			time_event *timeev = *timeevset.begin();
			/** judge if all timeout > now, if not then 
			 *  means have some active timeout event need to be 
			 *  processed, so do not dispatch, because dispatch 
			 *  will wait until the next timeout appear */
			if (timercmp(&(timeev->_timeout), &now, >))
			{
				timersub(&(timeev->_timeout), &now, &off);
				/* attach timeout is off */
				res = this->dispatch(&off);
			}
		}

		if (res == -1)
		{
			std::cout << "dispatch exit res=" << res << std::endl;
			return -1;
		}

		timeout_process();
		event_process_active();

		if (this->recalc(0) == -1)
		{
			std::cout << "recalc exited -1\n";
			return -1;
		}
	}
	std::cout << "exit with done=" << done << std::endl;
	return 0;
}

void event_base::timeout_process()
{
	std::cout << __func__ << std::endl;
	struct timeval now;
	gettimeofday(&now, NULL);

	time_event *ev;
	std::set<time_event *, cmp_timeev>::iterator i = timeevset.begin();
	while (i != timeevset.end())
	{
		ev = *i;
		if (timercmp(&ev->_timeout, &now, >))
			break;
		i = timeevset.erase(i);
		ev->activate(1);
	}
}

void event_base::event_process_active()
{
	std::cout << __func__ << std::endl;
	if (activequeues.empty())
		return;

	std::list<event *> &activeq = this->activequeues[0];
	for (auto &item : this->activequeues)
	{
		if (!item.empty())
		{
			activeq = item;
			break;
		}
	}
	if (!activeq.empty())
	{
		event *ev;
		std::list<event *>::iterator i = activeq.begin();
		while (i != activeq.end())
		{
			ev = *i;
			while (ev->_ncalls)
			{
				ev->_ncalls--;
				(*ev->_callback)(ev);
			}
			i = activeq.erase(i);
		}
	}
}

/** deal with signal **/
void event_base::evsignal_process()
{
	std::cout << __func__ << std::endl;
	short ncalls;
	signal_event *sigev = nullptr;
	std::list<signal_event *>::iterator i = signalqueue.begin();
	while (i != signalqueue.end())
	{
		sigev = *i;
		ncalls = evsigcaught[sigev->_sig];
		if (ncalls)
		{
			if (!(sigev->is_persistent()))
				i = signalqueue.erase(i);
			sigev->activate(ncalls);
		}
		i++;
	}

	memset(this->evsigcaught, 0, sizeof(evsigcaught));
	caught = 0;
}

int event_base::evsignal_recalc()
{
	std::cout << __func__ << std::endl;
	if (signalqueue.empty() && !needrecalc)
		return 0;
	needrecalc = 0;
	if (sigprocmask(SIG_BLOCK, &evsigmask, NULL) == -1)
		return -1;

	struct sigaction sa;
	/* Reinstall our signal handler. */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = this->handler;
	sa.sa_mask = this->evsigmask;
	sa.sa_flags |= SA_RESTART;

	for (const auto &ev : signalqueue)
	{
		std::cout << "_sig=" << ev->_sig << std::endl;
		if (ev->_sig < 0 || ev->_sig >= NSIG)
		{
			std::cout << "error _sig not set\n";
			exit(-1);
		}
		std::cout << "_sig=" << ev->_sig << std::endl;
		if (sigaction(ev->_sig, &sa, NULL) == -1)
			return -1;
	}
	return 0;
}

int event_base::evsignal_deliver()
{
	std::cout << __func__ << std::endl;
	if (signalqueue.empty())
		return 0;
	return sigprocmask(SIG_UNBLOCK, &evsigmask, NULL);
}

void event_base::readsig_cb(void *arg)
{
	std::cout << __func__ << std::endl;
	static char signals[100];
	rw_event *ev = (rw_event *)arg;
	int n = read(ev->_fd, signals, sizeof(signals));
	if (n == -1)
	{
		std::cout << __func__ << " err \n";
		exit(-1);
	}
	ev->add();
}

void event_base::handler(int sig)
{
	std::cout << __func__ << std::endl;
	evsigcaught[sig]++;
	caught = 1;

	write(ev_signal_pair[0], "a", 1);
}

} // namespace eve
