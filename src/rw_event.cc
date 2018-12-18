#include "rw_event.hh"
#include "event_base.hh"

#include <iostream>
namespace eve
{

rw_event::rw_event(event_base *base)
    : event(base)
{
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

void rw_event::add()
{
    if (!_read && !_write)
    {
        std::cerr << "warning: add rw_event with no write or read\n";
        return;
    }
    _write_enabled = _write;
    _read_enabled = _read; 

    this->base->add(this);
    this->base->fd_map_rw[this->fd] = this;
}

void rw_event::del()
{
    this->base->del(this);
    if (is_removeable())
        this->base->fd_map_rw.erase(this->fd);
}

void rw_event::add_read()
{
    _read_enabled = true;
    this->base->add(this);
    this->base->fd_map_rw[this->fd] = this;
}

void rw_event::add_write()
{
    _write_enabled = true;
    this->base->add(this);
    this->base->fd_map_rw[this->fd] = this;
}

void rw_event::del_read()
{
    _read_enabled = false;
    del();
}

void rw_event::del_write()
{
    _write_enabled = false;
    del();
}



} // namespace eve
