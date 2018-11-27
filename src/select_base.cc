#include <iostream>
#include <algorithm>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/select.h>

#include <unistd.h>

#include "select_base.hh"
#include "signal.hh"

namespace eve
{

select_base::select_base()
{
    std::cout << __func__ << std::endl;
    int fdsz = ((32 + NFDBITS) / sizeof(NFDBITS)) * sizeof(fd_mask);
    resize(fdsz);

    evsig = new evsignal(this);
}

select_base::~select_base()
{
}
/*
 * Called with the highest fd that we know about.  If it is 0, completely
 * recalculate everything.
 */
int select_base::recalc(int max)
{
    std::cout << __func__ << std::endl;
    return evsig->recalc();
}

int select_base::dispatch(struct timeval *tv)
{
    std::cout << "select_base::" << __func__ << std::endl;

    std::copy_n(this->event_readset_in.begin(), this->event_fdsz, event_readset_out.begin());
    std::copy_n(event_writeset_in.begin(), event_fdsz, event_writeset_out.begin());

    if (evsig->deliver() == -1)
    {
        std::cout << "evsig->deliver() error\n";
        return -1;
    }

    int res = select(event_fds, &event_readset_out[0], &event_writeset_out[0], NULL, tv);

    if (evsig->recalc() == -1)
    {
        std::cout << "evsig->recalc() error\n";
        return -1;
    }

    if (res == -1)
    {
        std::cout << "select res=-1\n";
        if (errno != EINTR)
        {
            std::cout << "select error errno=" << errno << std::endl;
            return -1;
        }
        evsig->process();
        return 0;
    }
    else if (evsignal::caught)
    {
        std::cout << "evsignal caught=1" << std::endl;
        evsig->process();
    }

    for (int i = 0; i <= event_fds; ++i)
    {
        event *r_ev = nullptr, *w_ev = nullptr;
        res = 0;
        if (FD_ISSET(i, &event_readset_out[0]))
        {
            r_ev = this->event_r_by_fd[i];
            res |= EV_READ;
        }
        if (FD_ISSET(i, &event_writeset_out[0]))
        {
            w_ev = event_w_by_fd[i];
            res |= EV_WRITE;
        }
        if (r_ev && (res & r_ev->ev_events))
        {
            if (!(r_ev->ev_events & EV_PERSIST))
                this->del(r_ev);
            this->activate(r_ev, res & r_ev->ev_events, 1);
        }
        if (w_ev && w_ev != r_ev && (res & w_ev->ev_events))
        {
            if (!(w_ev->ev_events & EV_PERSIST))
                this->del(w_ev);
            this->activate(w_ev, res & w_ev->ev_events, 1);
        }
    }

    return 0;
}

int select_base::resize(int fdsz)
{
    std::cout << __func__ << std::endl;
    int n_events = (fdsz / sizeof(fd_mask)) * NFDBITS;
    int n_events_old = (this->event_fdsz / sizeof(fd_mask)) * NFDBITS;

    // check
    event_readset_in.resize(fdsz);
    event_readset_out.resize(fdsz);
    event_writeset_in.resize(fdsz);
    event_writeset_out.resize(fdsz);

    this->event_fdsz = fdsz;

    return 0;
}

int select_base::add(event *ev)
{
    std::cout << "select_base::" << __func__ << " ev->fd=" << ev->ev_fd << std::endl;
    if (ev->ev_events & EV_SIGNAL)
        return evsig->add(ev);

    //check

    /*
     * Keep track of the highest fd, so that we can calculate the size
     * of the fd_sets for select(2)
     */
    if (this->event_fds < ev->ev_fd)
    {
        int fdsz = this->event_fdsz;
        if (fdsz < sizeof(fd_mask))
            fdsz = sizeof(fd_mask);

        int needsize = ((ev->ev_fd + NFDBITS) / NFDBITS) * sizeof(fd_mask);
        while (fdsz < needsize)
        {
            fdsz *= 2;
        }

        if (fdsz != this->event_fdsz)
        {
            this->resize(fdsz);
        }

        this->event_fdsz = ev->ev_fd;
    }

    if (ev->ev_events & EV_READ)
    {
        FD_SET(ev->ev_fd, &this->event_readset_in[0]);
        this->event_r_by_fd[ev->ev_fd] = ev;
    }
    if (ev->ev_events & EV_WRITE)
    {
        FD_SET(ev->ev_fd, &this->event_writeset_in[0]);
        this->event_w_by_fd[ev->ev_fd] = ev;
    }

    return 0;
}

int select_base::del(event *ev)
{
    std::cout << __func__ << std::endl;
    if (ev->ev_events & EV_SIGNAL)
        return evsig->del(ev);

    if (ev->ev_events & EV_READ)
    {
        FD_CLR(ev->ev_fd, &this->event_readset_in[0]);
        this->event_r_by_fd[ev->ev_fd] = nullptr;
    }

    if (ev->ev_events & EV_WRITE)
    {
        FD_CLR(ev->ev_fd, &this->event_writeset_in[0]);
        this->event_w_by_fd[ev->ev_fd] = nullptr;
    }

    return 0;
}

} // namespace eve
