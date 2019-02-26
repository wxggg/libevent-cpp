#include "poll_base.hh"
#include "rw_event.hh"

#include <assert.h>

namespace eve
{

int poll_base::recalc()
{
    // poll_check();
    return evsignal_recalc();
}

void poll_base::poll_check()
{
    rw_event *ev = nullptr;
    int fd = -1;
    for (auto kv : fd_map_poll)
    {
        fd = kv.first;
        assert(fd > 0);
        struct pollfd *pfd = kv.second;
        assert(pfd);
        assert(fd == pfd->fd);
        ev = fd_map_rw[fd];
        assert(ev);
        assert(fd == ev->fd);
        if (ev->is_readable())
            assert(pfd->events & POLLIN);
        if (ev->is_writable())
            assert(pfd->events & POLLOUT);
    }
}

int poll_base::dispatch(struct timeval *tv)
{
    // std::cout << __PRETTY_FUNCTION__ << std::endl;
    if (evsignal_deliver() == -1)
        return -1;

    int sec = -1;
    if (tv)
        sec = tv->tv_sec * 1000 + (tv->tv_usec + 999) / 1000;

    poll_check();

    int nfds = fd_map_poll.size();
    struct pollfd fds[nfds];
    int i = 0;
    for (const auto kv : fd_map_poll)
        fds[i++] = *kv.second;
        
    int res = poll(fds, nfds, sec);
    if (evsignal_recalc() == -1)
        return -1;

    if (res == -1)
    {
        if (errno != EINTR)
        {
            std::cerr << "poll\n";
            return -1;
        }
        evsignal_process();
        return 0;
    }
    else if (caught)
        evsignal_process();

    if (res == 0)
        return 0;

    int what = 0;
    rw_event *ev;
    for (i = 0; i < nfds; i++)
    {
        what = fds[i].revents;
        ev = fd_map_rw[fds[i].fd];
        if (what && ev)
        {
            ev->clear_active();
            /* if the file gets closed notify */
            if (what & (POLLHUP | POLLERR))
                what |= POLLIN | POLLOUT;
            if ((what & POLLIN) && ev->is_readable())
                ev->activate_read();
            if ((what & POLLOUT) && ev->is_writable())
                ev->activate_write();

            if (ev->is_read_active() || ev->is_write_active())
            {
                if (!ev->is_persistent())
                    ev->del();
                ev->activate(1);
            }
        }
    }
    return 0;
}

int poll_base::add(rw_event *ev)
{
    struct pollfd *pfd = fd_map_poll[ev->fd];
    if (!pfd)
    {
        pfd = new struct pollfd;
        pfd->fd = ev->fd;
        pfd->events = 0;
        pfd->revents = 0;
        fd_map_poll[ev->fd] = pfd;
    }

    if (ev->is_read_available())
        pfd->events |= POLLIN;
    if (ev->is_write_available())
        pfd->events |= POLLOUT;

    return 0;
}

int poll_base::del(rw_event *ev)
{
    if (ev->is_removeable())
    {
        delete fd_map_poll[ev->fd];
        fd_map_poll.erase(ev->fd);
    }
    else
    {
        struct pollfd *pfd = fd_map_poll[ev->fd];
        if (!pfd)
            return 1;
        if (!ev->is_read_available())
            pfd->events &= ~POLLIN;
        if (!ev->is_write_available())
            pfd->events &= ~POLLOUT;
    }

    return 0;
}

} // namespace eve
