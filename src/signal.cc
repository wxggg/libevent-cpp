#include "signal.hh"
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace eve
{

short evsignal::evsigcaught[NSIG];
volatile sig_atomic_t evsignal::caught = 0;
int evsignal::ev_signal_pair[2];
int evsignal::needrecalc = 0;
int evsignal::ev_signal_added = 0;

evsignal::evsignal(event_base *base)
{
	std::cout << __func__ << std::endl;
	sigemptyset(&evsigmask);
	/* 
	 * Our signal handler is going to write to one end of the socket
	 * pair to wake up our event loop.  The event loop then scans for
	 * signals that got delivered.
	 */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, ev_signal_pair) == -1)
	{
		std::cout << __func__ << " socketpair error\n";
	}

	// FD_CLOSEONEXEC() ??
	ev_signal = new event;
	ev_signal->set(base, ev_signal_pair[1], EV_READ, this->callback);
	ev_signal->ev_flags |= EVLIST_INTERNAL;

	this->ev_base = base;
}

/** public **/

int evsignal::add(event *ev)
{
	std::cout << __func__ << std::endl;
	if (ev->ev_events & (EV_READ | EV_WRITE))
	{
		std::cerr << __func__ << "RW err\n";
	}
	sigaddset(&this->evsigmask, ev->ev_fd);
	return 0;
}

int evsignal::del(event *ev)
{
	std::cout << __func__ << std::endl;
	sigdelset(&this->evsigmask, ev->ev_fd);
	this->needrecalc = 1;
	std::cout << "cut1" << std::endl;
	return sigaction(ev->ev_fd, (struct sigaction *)SIG_DFL, NULL);
}

int evsignal::recalc()
{
	std::cout << __func__ << std::endl;
	if (!ev_signal_added)
	{
		ev_signal_added = 1;
		this->ev_base->add_event(this->ev_signal, NULL);
	}

	if (this->ev_base->signalqueue.empty() && !needrecalc)
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

	for (auto ev : this->ev_base->signalqueue)
	{
		if (sigaction(ev->ev_fd, &sa, NULL) == -1)
			return -1;
	}
	return 0;
}

int evsignal::deliver()
{
	std::cout << __func__ << std::endl;
	if (this->ev_base->signalqueue.empty())
		return 0;

	return sigprocmask(SIG_UNBLOCK, &this->evsigmask, NULL);
}

void evsignal::process()
{
	std::cout << __func__ << std::endl;
	short ncalls;
	for (auto ev : this->ev_base->signalqueue)
	{
		ncalls = evsigcaught[ev->ev_fd];
		if (ncalls)
		{
			if (!(ev->ev_events & EV_PERSIST))
				this->ev_base->del_event(ev);
			this->ev_base->activate(ev, EV_SIGNAL, ncalls);
		}
	}

	memset(this->evsigcaught, 0, sizeof(evsigcaught));
	caught = 0;
}

/** private **/

void evsignal::callback(int fd, short what, void *arg)
{
	std::cout << __func__ << std::endl;
	static char signals[100];
	event *ev = (event *)arg;
	int n = read(fd, signals, sizeof(signals));
	if (n == -1)
	{
		std::cout << __func__ << " err \n";
		exit(-1);
	}
	ev->ev_base->add_event(ev, NULL);
}

void evsignal::handler(int sig)
{
	std::cout << __func__ << std::endl;
	evsigcaught[sig]++;
	caught = 1;

	/* Wake up our notification mechanism */
	write(ev_signal_pair[0], "a", 1);
}

} // namespace eve
