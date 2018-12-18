#include "event_base.hh"
#include "signal_event.hh"

#include <signal.h>

#include <iostream>
namespace eve
{
    
signal_event::signal_event(event_base *base)
    :event(base)
{
}

void signal_event::add()
{
    this->base->signalqueue.push_back(this);
    sigaddset(&this->base->evsigmask, sig);
}

void signal_event::del()
{
    this->base->signalqueue.remove(this);
    sigdelset(&this->base->evsigmask, sig);
    sigaction(sig, (struct sigaction*)SIG_DFL, NULL);
}


} // eve
