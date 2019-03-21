#include <event_base.hh>
#include <rw_event.hh>
#include <signal_event.hh>
#include <time_event.hh>
// #include <util_network.hh>
#include <logger.hh>

#include <string>
#include <cstring>
#include <iostream>
#include <algorithm>

namespace eve
{

volatile sig_atomic_t event_base::caught = 0;
std::vector<int> event_base::sigcaught = {};
int event_base::needrecalc = 0;

bool cmp_timeev::operator()(std::shared_ptr<time_event> const &lhs, std::shared_ptr<time_event> const &rhs) const
{
	return timercmp(&lhs->timeout, &rhs->timeout, <);
}

event_base::event_base()
{
	priority_init(1); // default have 1 activequeues
	sigemptyset(&evsigmask);

	sigcaught.resize(NSIG);
}

int event_base::add_event(const std::shared_ptr<event> &ev)
{
	if (std::dynamic_pointer_cast<signal_event>(ev))
	{
		auto sigev = std::dynamic_pointer_cast<signal_event>(ev);
		signalList.push_back(sigev);
		return sigaddset(&evsigmask, sigev->sig);
	}
	else if (std::dynamic_pointer_cast<time_event>(ev))
	{
		auto tev = std::dynamic_pointer_cast<time_event>(ev);
		timeSet.insert(tev);
		return 0;
	}
	else if (std::dynamic_pointer_cast<rw_event>(ev))
	{
		auto rw = std::dynamic_pointer_cast<rw_event>(ev);
		if (rw->is_removeable())
		{
			LOG_WARN << "add rw event with no READ or WRITE, please use enble_read() or enblae_write()";
		}
		fdMapRw[rw->fd] = rw;
		return add(rw);
	}
	else
	{
		LOG_ERROR << "no such event defined as " << typeid(ev).name();
		return -1;
	}
}

int event_base::remove_event(const std::shared_ptr<event> &ev)
{
	if (std::dynamic_pointer_cast<signal_event>(ev))
	{
		auto sigev = std::dynamic_pointer_cast<signal_event>(ev);
		signalList.remove(sigev);
		sigdelset(&evsigmask, sigev->sig);
		return sigaction(sigev->sig, (struct sigaction *)SIG_DFL, nullptr);
	}
	else if (std::dynamic_pointer_cast<time_event>(ev))
	{
		auto tev = std::dynamic_pointer_cast<time_event>(ev);
		timeSet.erase(tev);
		return 0;
	}
	else if (std::dynamic_pointer_cast<rw_event>(ev))
	{
		auto rw = std::dynamic_pointer_cast<rw_event>(ev);
		if (rw->is_removeable())
			fdMapRw.erase(rw->fd);
		return del(rw);
	}
	else
	{
		LOG_ERROR << "no such event defined as " << typeid(ev).name();
		return -1;
	}
}

void event_base::clean_event(const std::shared_ptr<event> &ev)
{
	remove_event(ev);
	callbackMap.erase(ev->id);
}

void event_base::activate(std::shared_ptr<event> ev, short ncalls)
{
	ev->ncalls = ncalls;
	activeQueues[ev->pri].push(ev);
	ev->set_active();
}

void event_base::activate_read(std::shared_ptr<rw_event> ev)
{
	ev->set_active_read();
	if (ev->is_removeable())
		remove_event(ev);
}

void event_base::activate_write(std::shared_ptr<rw_event> ev)
{
	ev->set_active_write();
	if (ev->is_removeable())
		remove_event(ev);
}

int event_base::priority_init(int npriorities)
{
	if (npriorities == active_queue_size() || npriorities < 1)
		return 0;
	if (!activeQueues.empty() && npriorities != active_queue_size())
		activeQueues.clear();

	activeQueues.resize(npriorities);
	return 0;
}

int event_base::loop()
{
	int res = __loop();
	__clean_up();
	return res;
}

int event_base::__loop()
{
	/* Calculate the initial events that we are waiting for */
	if (this->recalc() == -1)
		return -1;

	int done = 0;
	while (!done)
	{
		LOG << "loop" << i++;
		/* Terminate the loop if we have been asked to */
		if (this->_terminated)
		{
			LOG << "[event] event got terminated";
			_terminated = false;
			break;
		}

		int nactive_events = active_event_size();

		/* If we have no events, we just exit */
		if (signalList.empty() && timeSet.empty() && !rw_event_size() && !nactive_events)
		{
			LOG << "[event] have no events, just exit";
			return 1;
		}

		int res;
		struct timeval off;
		if (_loop_nonblock) // non block
		{
			timerclear(&off);
			res = this->dispatch(&off);
		}
		else if (!nactive_events)
		{
			if (timeSet.empty()) // no time event
				res = this->dispatch(nullptr);
			else // has time event
			{
				struct timeval now;
				gettimeofday(&now, nullptr);
				auto tev = *timeSet.begin();
				if (timercmp(&(tev->timeout), &now, >)) // no time event time out
				{
					timersub(&(tev->timeout), &now, &off);
					res = this->dispatch(&off);
				}
			}
		}

		if (res == -1)
		{
			LOG_ERROR << "[event] dispatch exit res=" << res;
			return -1;
		}

		if (!timeSet.empty())
			process_timeout_events();

		if (active_event_size())
		{
			process_active_events();
			if (_loop_once && !active_event_size())
				done = 1;
		}
		else if (_loop_nonblock)
			done = 1;

		if (this->recalc() == -1)
			return -1;
	}
	// std::cout << "loop exit with done=" << done << std::endl;
	return 0;
}

void event_base::__clean_up()
{
	callbackMap.clear();

	int n = activeQueues.size();
	activeQueues.clear();
	activeQueues.resize(n);
	signalList.clear();
	timeSet.clear();
	fdMapRw.clear();
}

void event_base::process_timeout_events()
{
	struct timeval now;
	gettimeofday(&now, nullptr);

	auto i = timeSet.begin();
	while (i != timeSet.end())
	{
		auto ev = *i;
		if (timercmp(&ev->timeout, &now, >))
			break;
		i = timeSet.erase(i);
		activate(ev, 1);
	}
}

void event_base::process_active_events()
{
	if (activeQueues.empty())
		return;

	auto it = std::find_if(activeQueues.begin(), activeQueues.end(),
						   [](decltype(activeQueues[0]) q) { return !q.empty(); });
	auto &q = *it;

	while (!q.empty())
	{
		auto ev = q.front();
		q.pop();
		while (ev->ncalls)
		{
			--ev->ncalls;
			auto f = callbackMap[ev->id];
			if (f)
				(*f)();
		}
		ev->clear_active();
	}
}

/** deal with signal **/
void event_base::evsignal_process()
{
	short ncalls;
	auto i = signalList.begin();
	while (i != signalList.end())
	{
		auto ev = *i;
		ncalls = sigcaught[ev->sig];
		if (ncalls)
		{
			if (!(ev->is_persistent()))
				i = signalList.erase(i);
			activate(ev, ncalls);
		}
		i++;
	}

	std::fill(sigcaught.begin(), sigcaught.end(), 0);
	caught = 0;
}

int event_base::evsignal_recalc()
{
	if (signalList.empty() && !needrecalc)
		return 0;
	needrecalc = 0;
	if (sigprocmask(SIG_BLOCK, &evsigmask, nullptr) == -1)
	{
		LOG_ERROR << "sigprocmask error";
		return -1;
	}

	struct sigaction sa;
	/* Reinstall our signal handler. */
	std::memset(&sa, 0, sizeof(sa));
	sa.sa_handler = this->handler;
	sa.sa_mask = this->evsigmask;
	sa.sa_flags |= SA_RESTART;

	for (const auto &ev : signalList)
	{
		if (ev->sig < 0 || ev->sig >= NSIG)
		{
			LOG_ERROR << "sig not set";
			exit(-1);
		}
		if (sigaction(ev->sig, &sa, nullptr) == -1)
		{
			LOG_ERROR << "sigaction error";
			return -1;
		}
	}
	return 0;
}

int event_base::evsignal_deliver()
{
	if (signalList.empty())
		return 0;
	int res = sigprocmask(SIG_UNBLOCK, &evsigmask, nullptr);
	if (res == -1)
		LOG_ERROR << "sigprocmask error";
	return res;
}

void event_base::handler(int sig)
{
	sigcaught[sig]++;
	caught = 1;
}

} // namespace eve
