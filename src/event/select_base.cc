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

#define MAX_SELECT_FD 1024

select_base::select_base()
{
    int fdsz = ((32 + NFDBITS) / sizeof(NFDBITS)) * sizeof(fd_mask);
    resize(fdsz);
}

select_base::~select_base()
{
    free(event_readset_in);
    free(event_writeset_in);
    free(event_readset_out);
    free(event_writeset_out);
}

void select_base::check_fdset()
{
    LOG_DEBUG;
    bool iread = false, iwrite = false;

    for (auto kv : fdMapRw)
    {
        // std::cout << "kv:" << kv.first << " " << kv.second << std::endl;
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
            // assert(kv.second->_fd == kv.first);
        }
        if (iread)
            assert(kv.second->is_readable());
        if (iwrite)
            assert(kv.second->is_writeable());
    }
}

int select_base::recalc()
{
    // check_fdset();
    return evsignal_recalc();
}

int select_base::dispatch(struct timeval *tv)
{
    memcpy(event_readset_out, event_readset_in, _fdsz);
    memcpy(event_writeset_out, event_writeset_in, _fdsz);

    check_fdset();

    if (evsignal_deliver() == -1)
        return -1;

    int res = select(_fds + 1, event_readset_out, event_writeset_out, nullptr, tv);

    // check_fdset();

    if (evsignal_recalc() == -1)
        return -1;

    if (res == -1)
    {
        if (errno != EINTR)
        {
            LOG_ERROR << "select error errno=" << errno;
            return -1;
        }
        evsignal_process();
        return 0;
    }
    else if (caught)
    {
        evsignal_process();
    }

    // check_fdset();
    bool iread, iwrite;
    for (auto kv : fdMapRw)
    {
        iread = iwrite = false;
        if (FD_ISSET(kv.first, event_readset_out))
            iread = true;
        if (FD_ISSET(kv.first, event_writeset_out))
            iwrite = true;

        if ((iread || iwrite) && kv.second)
        {
            auto ev = kv.second;
            ev->clear_active();
            if (iread && ev->is_readable())
                ev->set_active_read();
            if (iwrite && ev->is_writeable())
                ev->set_active_write();

            if (ev->is_read_active() || ev->is_write_active())
            {
                if (!ev->is_persistent())
                    remove_event(ev);
                activate(ev, 1);
            }
        }
    }

    // check_fdset();

    return 0;
}

int select_base::resize(int fdsz)
{
    // if (event_readset_in)
    //     check_fdset();

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

    // check_fdset();

    return 0;
}

int select_base::add(std::shared_ptr<rw_event> ev)
{
    if (ev->fd > MAX_SELECT_FD)
    {
        LOG_WARN << "select added fd>" << MAX_SELECT_FD << " , reset MAX_SELECT_FD if needed";
        exit(-1);
    }
    /*
     * Keep track of the highest fd, so that we can calculate the size
     * of the fd_sets for select(2)
     */
    if (this->_fds < ev->fd)
        this->_fds = ev->fd;
    if (this->_fdsz - 1 < ev->fd)
    {
        int fdsz = this->_fdsz;
        if (fdsz < static_cast<int>(sizeof(fd_mask)))
            fdsz = sizeof(fd_mask);

        int needsize = ((ev->fd + NFDBITS) / NFDBITS) * sizeof(fd_mask);
        while (fdsz < needsize)
            fdsz *= 2;

        if (fdsz != this->_fdsz)
            this->resize(fdsz);
    }

    if (ev->is_readable())
        FD_SET(ev->fd, event_readset_in);
    if (ev->is_writeable())
        FD_SET(ev->fd, event_writeset_in);

    return 0;
}

int select_base::del(std::shared_ptr<rw_event> ev)
{
    // check_fdset();

    if (!ev->is_readable())
        FD_CLR(ev->fd, event_readset_in);

    if (!ev->is_writeable())
        FD_CLR(ev->fd, event_writeset_in);
    // check_fdset();

    return 0;
}

} // namespace eve
