#include "event_base.hh"
#include "signal_event.hh"

#include <signal.h>

#include <iostream>
namespace eve
{
    
signal_event::signal_event(event_base *base)
    :event(base)
{
    std::cout << __func__ << std::endl;
}

void signal_event::add()
{
	std::cout << __PRETTY_FUNCTION__ << std::endl;
    this->base->signalqueue.push_back(this);
    sigaddset(&this->base->evsigmask, _sig);
}

void signal_event::del()
{
	std::cout << __PRETTY_FUNCTION__ << std::endl;
    this->base->signalqueue.remove(this);
    sigdelset(&this->base->evsigmask, _sig);
    sigaction(_sig, (struct sigaction*)SIG_DFL, NULL);
}


} // eve
