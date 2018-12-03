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
    std::cout << __func__ << std::endl;
    bool iread = false, iwrite = false;

    for (auto kv : fd_map_rw)
    {
        std::cout << "kv:" << kv.first << " " << kv.second << std::endl;
        iread = false, iwrite = false;
        if (FD_ISSET(kv.first, event_readset_in))
            iread = true;
        if (FD_ISSET(kv.first, event_writeset_in))
            iwrite = true;

        if (!iread && !iwrite)
        {
            assert(!kv.second);
            continue;
        }
        else
        {
            assert(kv.second);
            assert(kv.second->_fd == kv.first);
        }
        if (iread)
            assert(kv.second->is_readable());
        if (iwrite)
            assert(kv.second->is_writable());
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

    memcpy(event_readset_out, event_readset_in, _fdsz);
    memcpy(event_writeset_out, event_writeset_in, _fdsz);

    check_fdset();

    if (evsignal_deliver() == -1)
    {
        std::cout << "evsig->deliver() error\n";
        return -1;
    }

    int res = select(_fds + 1, event_readset_out, event_writeset_out, NULL, tv);

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
    bool iread, iwrite;
    rw_event *ev;

    for (auto kv : fd_map_rw)
    {
        iread = iwrite = false;
        if (FD_ISSET(kv.first, event_readset_out))
            iread = true;
        if (FD_ISSET(kv.first, event_writeset_out))
            iwrite = true;

        if ((iread || iwrite) && kv.second)
        {
            ev = kv.second;
            if (iread && ev->is_readable())
                ev->set_active_read();
            if (iwrite && ev->is_writable())
                ev->set_active_write();

            if (ev->has_active_read() || ev->has_active_write())
            {
                if (!ev->is_persistent())
                    ev->del();
                ev->activate(1);
            }
        }
    }

    check_fdset();

    return 0;
}

int select_base::resize(int fdsz)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

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

    memset(event_readset_in + _fdsz, 0, fdsz - _fdsz);
    memset(event_writeset_in + _fdsz, 0, fdsz - _fdsz);

    this->_fdsz = fdsz;

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
    if (this->_fds < ev->_fd)
        this->_fds = ev->_fd;
    if (this->_fdsz - 1 < ev->_fd)
    {
        int fdsz = this->_fdsz;
        if (fdsz < sizeof(fd_mask))
            fdsz = sizeof(fd_mask);

        int needsize = ((ev->_fd + NFDBITS) / NFDBITS) * sizeof(fd_mask);
        while (fdsz < needsize)
            fdsz *= 2;

        if (fdsz != this->_fdsz)
            this->resize(fdsz);
    }

   if (ev->is_readable())
    {
        FD_SET(ev->_fd, event_readset_in);
        this->fd_map_rw[ev->_fd] = ev;
    }
    if (ev->is_writable())
    {
        FD_SET(ev->_fd, event_writeset_in);
        this->fd_map_rw[ev->_fd] = ev;
    }

    return 0;
}

int select_base::del(rw_event *ev)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    check_fdset();
    if (ev->is_readable())
    {
        FD_CLR(ev->_fd, event_readset_in);
        fd_map_rw.erase(ev->_fd);
    }

    if (ev->is_writable())
    {
        FD_CLR(ev->_fd, event_writeset_in);
        fd_map_rw.erase(ev->_fd);
    }
    check_fdset();

    return 0;
}

} // namespace eve
