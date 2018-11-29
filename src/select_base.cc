#include <iostream>
#include <algorithm>

#include <sys/types.h>
#include <sys/times.h>
#include <sys/select.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "select_base.hh"
#include "rw_event.hh"

namespace eve
{

select_base::select_base()
    : event_base()
{
    std::cout << __func__ << std::endl;
    int fdsz = ((32 + NFDBITS) / sizeof(NFDBITS)) * sizeof(fd_mask);
    resize(fdsz);
}

select_base::~select_base()
{
    std::cout << __func__ << std::endl;
    free(event_readset_in);
    free(event_writeset_in);
    free(event_readset_out);
    free(event_writeset_out);
}

void select_base::check_fdset()
{
    for (int i = 0; i <= event_fds; ++i)
    {
        if (FD_ISSET(i, event_readset_in))
        {
            assert(event_r_by_fd[i]);
            assert(event_r_by_fd[i]->_events & EV_READ);
            assert(event_r_by_fd[i]->_fd == i);
        }
        else
        {
            assert(!event_r_by_fd[i]);
        }

        if (FD_ISSET(i, event_writeset_in))
        {
            assert(event_w_by_fd[i]);
            assert(event_w_by_fd[i]);
            assert(event_w_by_fd[i]->_events & EV_WRITE);
            assert(event_w_by_fd[i]->_fd == i);
        }
        else
        {
            assert(!event_w_by_fd[i]);
        }
    }
}

int select_base::recalc(int max)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    check_fdset();
    return evsignal_recalc();
}

int select_base::dispatch(struct timeval *tv)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    check_fdset();

    memcpy(event_readset_out, event_readset_in, event_fdsz);
    memcpy(event_writeset_out, event_writeset_in, event_fdsz);

    check_fdset();

    if (evsignal_deliver() == -1)
    {
        std::cout << "evsig->deliver() error\n";
        return -1;
    }

    int res = select(event_fds, event_readset_out, event_writeset_out, NULL, tv);

    check_fdset();

    if (evsignal_recalc() == -1)
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
        evsignal_process();
        return 0;
    }
    else if (caught)
    {
        std::cout << "evsignal caught=1" << std::endl;
        evsignal_process();
    }

    check_fdset();
    for (int i = 0; i <= event_fds; ++i)
    {
        rw_event *r_ev = nullptr, *w_ev = nullptr;
        res = 0;
        if (FD_ISSET(i, event_readset_out))
        {
            r_ev = this->event_r_by_fd[i];
            res |= EV_READ;
        }
        if (FD_ISSET(i, event_writeset_out))
        {
            w_ev = event_w_by_fd[i];
            res |= EV_WRITE;
        }
        if (r_ev && (res & r_ev->_events))
        {
            if (!(r_ev->_events & EV_PERSIST))
                this->del(r_ev);
            r_ev->activate(res & r_ev->_events, 1);
        }
        if (w_ev && w_ev != r_ev && (res & w_ev->_events))
        {
            if (!(w_ev->_events & EV_PERSIST))
                this->del(w_ev);
            w_ev->activate(res & r_ev->_events, 1);
        }
    }
    check_fdset();

    return 0;
}

int select_base::resize(int fdsz)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    int n_events = (fdsz / sizeof(fd_mask)) * NFDBITS;
    int n_events_old = (this->event_fdsz / sizeof(fd_mask)) * NFDBITS;

    if (event_readset_in)
        check_fdset();

    fd_set *newset = nullptr;
    newset = (fd_set *)realloc(event_readset_in, fdsz);
    event_readset_in = newset;

    newset = (fd_set *)realloc(event_writeset_in, fdsz);
    event_writeset_in = newset;

    newset = (fd_set *)realloc(event_readset_out, fdsz);
    event_readset_out = newset;

    newset = (fd_set *)realloc(event_writeset_out, fdsz);
    event_writeset_out = newset;

    memset(event_readset_in + event_fdsz, 0, fdsz - event_fdsz);
    memset(event_writeset_in + event_fdsz, 0, fdsz - event_fdsz);
    memset(event_readset_out + event_fdsz, 0, fdsz - event_fdsz);
    memset(event_writeset_out + event_fdsz, 0, fdsz - event_fdsz);

    this->event_fdsz = fdsz;

    check_fdset();

    return 0;
}

int select_base::add(rw_event *ev)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    /*
     * Keep track of the highest fd, so that we can calculate the size
     * of the fd_sets for select(2)
     */
    if (this->event_fds < ev->_fd)
    {
        int fdsz = this->event_fdsz;
        if (fdsz < sizeof(fd_mask))
            fdsz = sizeof(fd_mask);

        int needsize = ((ev->_fd + NFDBITS) / NFDBITS) * sizeof(fd_mask);
        while (fdsz < needsize)
        {
            fdsz *= 2;
        }

        if (fdsz != this->event_fdsz)
        {
            this->resize(fdsz);
        }

        this->event_fdsz = ev->_fd;
    }

    if (ev->_events & EV_READ)
    {
        FD_SET(ev->_fd, event_readset_in);
        this->event_r_by_fd[ev->_fd] = ev;
    }
    if (ev->_events & EV_WRITE)
    {
        FD_SET(ev->_fd, event_writeset_in);
        this->event_w_by_fd[ev->_fd] = ev;
    }

    return 0;
}

int select_base::del(rw_event *ev)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    check_fdset();
    if (ev->_events & EV_READ)
    {
        FD_CLR(ev->_fd, event_readset_in);
        this->event_r_by_fd[ev->_fd] = nullptr;
    }

    if (ev->_events & EV_WRITE)
    {
        FD_CLR(ev->_fd, event_writeset_in);
        this->event_w_by_fd[ev->_fd] = nullptr;
    }
    check_fdset();

    return 0;
}

} // namespace eve
