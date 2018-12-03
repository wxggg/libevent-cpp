#include "poll_base.hh"
#include "rw_event.hh"

#include <assert.h>

namespace eve
{

poll_base::poll_base()
    : event_base()
{
}

poll_base::~poll_base()
{
}

int poll_base::recalc(int max)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    poll_check();
    return evsignal_recalc();
}

void poll_base::poll_check()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    rw_event *ev = nullptr;
    struct pollfd *pfd = nullptr;
    int fd = -1;
    for (auto kv : fd_map_poll)
    {
        fd = kv.first;
        assert(fd > 0);
        pfd = kv.second;
        assert(pfd);
        assert(fd == pfd->fd);
        ev = fd_map_rw[fd];
        assert(ev);
        assert(fd == ev->_fd);
        if (ev->is_readable())
            assert(pfd->events & POLLIN);
        if (ev->is_writable())
            assert(pfd->events & POLLOUT);
    }
}

int poll_base::dispatch(struct timeval *tv)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
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
            /* if the file gets closed notify */
            if (what & (POLLHUP | POLLERR))
                what |= POLLIN | POLLOUT;
            if ((what & POLLIN) && ev->is_readable())
                ev->set_active_read();
            if ((what & POLLOUT) && ev->is_writable())
                ev->set_active_write();

            if (ev->has_active_read() || ev->has_active_write())
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
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    fd_map_rw[ev->_fd] = ev;
    struct pollfd *pfd = new struct pollfd;
    pfd->fd = ev->_fd;
    pfd->events = 0;
    pfd->revents = 0;
    fd_map_poll[ev->_fd] = pfd;

    if (ev->is_readable())
        pfd->events |= POLLIN;
    if (ev->is_writable())
        pfd->events |= POLLOUT;

    return 0;
}

int poll_base::del(rw_event *ev)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    delete fd_map_poll[ev->_fd];
    fd_map_poll.erase(ev->_fd);
    fd_map_rw.erase(ev->_fd);
    return 0;
}

} // namespace eve
