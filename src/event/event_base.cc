#include <event_base.hh>
#include <rw_event.hh>
#include <signal_event.hh>
#include <time_event.hh>
#include <util_network.hh>

#include <string>
#include <cstring>
#include <iostream>

namespace eve
{

volatile sig_atomic_t event_base::caught = 0;
std::vector<int> event_base::sigcaught = {};
std::pair<int, int> event_base::signal_pair;
int event_base::needrecalc = 0;

bool cmp_timeev::operator()(time_event *const &lhs, time_event *const &rhs) const
{
	return timercmp(&lhs->timeout, &rhs->timeout, <);
}

event_base::event_base()
{
	priority_init(1); // default have 1 activequeues
	sigemptyset(&evsigmask);

	signal_pair = get_fdpair();
	sigcaught.resize(NSIG);

	// readsig = new rw_event(this);
	// readsig->set(signal_pair.second, READ, readsig_cb);
	// readsig->add();
}

int event_base::priority_init(int npriorities)
{
	if (npriorities == static_cast<int>(activequeues.size()) || npriorities < 1)
		return 0;
	if (!activequeues.empty() && npriorities != static_cast<int>(activequeues.size()))
	{
		for (auto item : activequeues)
			item.clear();
		activequeues.clear();
	}

	activequeues.resize(npriorities);
	return 0;
}

int event_base::loop()
{
	/* Calculate the initial events that we are waiting for */
	if (this->recalc() == -1)
		return -1;

	int done = 0;
	static int i = 0;
	while (!done && i++ < 1000)
	{
		std::cout << "\n[event]"
				  << " loop" << i << std::endl;
		/* Terminate the loop if we have been asked to */
		if (this->_terminated)
		{
			std::cout << "[event] event got terminated\n";
			set_terminated(false);
			break;
		}

		int nactive_events = count_active_events();

		/* If we have no events, we just exit */
		if (signalqueue.empty() && timeevset.empty() && !count_rw_events() && !nactive_events)
		{
			std::cout << "[event] have no events, just exit\n";
			return 1;
		}

		// std::cout<<"timev:"<<timeevset.size()<<std::endl;

		// std::cout<<count_rw_events()<<":"<<nactive_events<<std::endl;

		int res;
		struct timeval off;
		// struct timeval *tv = nullptr;
		if (_loop_nonblock) // non block
		{
			timerclear(&off);
			res = this->dispatch(&off);
		}
		else if (!nactive_events)
		{
			if (timeevset.empty()) // no time event
				res = this->dispatch(nullptr);
			else // has time event
			{
				struct timeval now;
				gettimeofday(&now, nullptr);
				time_event *timeev = *timeevset.begin();
				if (timercmp(&(timeev->timeout), &now, >)) // no time event time out
				{
					timersub(&(timeev->timeout), &now, &off);
					res = this->dispatch(&off);
				}
			}
		}

		if (res == -1)
		{
			std::cout << "[event] dispatch exit res=" << res << std::endl;
			return -1;
		}

		if (!timeevset.empty())
			timeout_process();

		if (count_active_events())
		{
			event_process_active();
			if (_loop_once && !count_active_events())
				done = 1;
		}
		else if (_loop_nonblock)
			done = 1;

		if (this->recalc() == -1)
		{
			std::cout << "[event] recalc exited -1\n";
			return -1;
		}
	}
	// std::cout << "loop exit with done=" << done << std::endl;
	return 0;
}

void event_base::timeout_process()
{
	struct timeval now;
	gettimeofday(&now, nullptr);

	time_event *ev;
	std::set<time_event *, cmp_timeev>::iterator i = timeevset.begin();
	while (i != timeevset.end())
	{

		ev = *i;
		if (timercmp(&ev->timeout, &now, >))
			break;
		i = timeevset.erase(i);
		ev->activate(1);
	}
}

void event_base::event_process_active()
{
	if (activequeues.empty())
		return;

	std::list<event *> &activeq = this->activequeues[0];
	int i = 0;
	for (auto &item : this->activequeues)
	{
		if (!item.empty())
		{
			activeq = item;
			break;
		}
		i++;
	}
	if (!activeq.empty())
	{
		event *ev;
		std::list<event *>::iterator i = activeq.begin();
		while (i != activeq.end())
		{
			ev = *i;
			while (ev->ncalls)
			{
				ev->ncalls--;
				// ev->callback(ev);
				(*ev->pcb)();
			}
			i = activeq.erase(i);
			ev->clear_active();
		}
	}
}

/** deal with signal **/
void event_base::evsignal_process()
{
	short ncalls;
	signal_event *sigev = nullptr;
	std::list<signal_event *>::iterator i = signalqueue.begin();
	while (i != signalqueue.end())
	{
		sigev = *i;
		ncalls = sigcaught[sigev->sig];
		if (ncalls)
		{
			if (!(sigev->is_persistent()))
				i = signalqueue.erase(i);
			sigev->activate(ncalls);
		}
		i++;
	}

	std::fill(sigcaught.begin(), sigcaught.end(), 0);
	caught = 0;
}

int event_base::evsignal_recalc()
{
	if (signalqueue.empty() && !needrecalc)
		return 0;
	needrecalc = 0;
	if (sigprocmask(SIG_BLOCK, &evsigmask, nullptr) == -1)
		return -1;

	struct sigaction sa;
	/* Reinstall our signal handler. */
	std::memset(&sa, 0, sizeof(sa));
	sa.sa_handler = this->handler;
	sa.sa_mask = this->evsigmask;
	sa.sa_flags |= SA_RESTART;

	for (const auto &ev : signalqueue)
	{
		if (ev->sig < 0 || ev->sig >= NSIG)
		{
			std::cout << "error sig not set\n";
			exit(-1);
		}
		if (sigaction(ev->sig, &sa, nullptr) == -1)
			return -1;
	}
	return 0;
}

int event_base::evsignal_deliver()
{
	if (signalqueue.empty())
		return 0;
	return sigprocmask(SIG_UNBLOCK, &evsigmask, nullptr);
}

void event_base::readsig_cb()
{
	std::cout << __func__ << std::endl;
	// static char signals[100];
	// rw_event *ev = (rw_event *)argev;
	// int n = read(ev->fd, signals, sizeof(signals));
	// if (n == -1)
	// {
	// 	std::cout << __func__ << " err \n";
	// 	exit(-1);
	// }
	// ev->add();
}

void event_base::handler(int sig)
{
	std::cout << __func__ << std::endl;
	sigcaught[sig]++;
	caught = 1;

	// write(ev_signal_pair[0], "a", 1);
}

} // namespace eve
