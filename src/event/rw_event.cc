#include <rw_event.hh>
#include <event_base.hh>
#include <util_linux.hh>

#include <iostream>

namespace eve
{

rw_event::~rw_event()
{
    del_read();
    del_write();
    if (fd != -1)
        closefd(fd);
}

void rw_event::activate_read()
{
    _active_read = true;
    if (!is_persistent())
        del_read();
}

void rw_event::activate_write()
{
    _active_write = true;
    if (!is_persistent())
        del_write();
}

int rw_event::add()
{
    if (!_read && !_write)
    {
        std::cerr << "warning: add rw_event with no write or read\n";
        return -1;
    }
    _write_enabled = _write;
    _read_enabled = _read;

    this->base->fd_map_rw[this->fd] = this;
    return this->base->add(this);
}

int rw_event::del()
{
    int res = this->base->del(this);
    if (is_removeable())
    {
        this->base->fd_map_rw.erase(this->fd);
    }
    return res;
}

int rw_event::add_read()
{
    _read_enabled = true;
    this->base->fd_map_rw[this->fd] = this;
    return this->base->add(this);
}

int rw_event::add_write()
{
    _write_enabled = true;
    this->base->fd_map_rw[this->fd] = this;
    return this->base->add(this);
}

int rw_event::del_read()
{
    _read_enabled = false;
    return del();
}

int rw_event::del_write()
{
    _write_enabled = false;
    return del();
}

} // namespace eve
