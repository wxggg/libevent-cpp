#pragma once

#include <signal.h>
#include "event.hh"

namespace eve
{

class evsignal
{
  private:
	sigset_t evsigmask;

  public:
	static short evsigcaught[NSIG];
	static volatile sig_atomic_t caught;
	static int ev_signal_pair[2];
	static int needrecalc;
	static int ev_signal_added;

	event *ev_signal;

	event_base *ev_base;

  public:
	evsignal(event_base *base);
	~evsignal() {}

	int add(event *ev);
	int del(event *ev);
	int recalc();
	int deliver();
	void process();

  private:
	/* Callback for when the signal handler write a byte to our signaling socket */
	static void handler(int sig);
	static void callback(int fd, short what, void *arg);
};

} // namespace eve
