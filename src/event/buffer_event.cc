#include <buffer_event.hh>
#include <event_base.hh>

namespace eve
{

buffer_event::buffer_event(std::shared_ptr<event_base> base, int fd)
    : base(base)
{
    input = std::make_shared<buffer>();
    output = std::make_shared<buffer>();
    ev = std::make_shared<rw_event>(base, fd, NONE);
    base->register_callback(ev, rw_callback, this);
}
buffer_event::~buffer_event()
{
    auto base = get_base();
    if (base)
        base->clean_event(ev);
}

size_t buffer_event::write(void *data, size_t size)
{
    int res = output->push_back(data, size);

    if (res == -1)
        return -1;

    if (size > 0)
    {
        ev->enable_write();
        get_base()->add_event(ev);
    }
    return res;
}

size_t buffer_event::read(void *data, size_t size)
{
    return input->pop_front(data, size);
}

void buffer_event::add_read_event()
{
    ev->enable_read();
    get_base()->add_event(ev);
}

void buffer_event::add_write_event()
{
    ev->enable_write();
    get_base()->add_event(ev);
}

void buffer_event::remove_read_event()
{
    ev->disable_read();
    get_base()->remove_event(ev);
}

void buffer_event::remove_write_event()
{
    ev->disable_write();
    get_base()->remove_event(ev);
}

void buffer_event::clean_event()
{
    ev->disable_read();
    ev->disable_write();
    get_base()->clean_event(ev);
}

void buffer_event::rw_callback(buffer_event *bev)
{
    int res = 0;
    auto ev = bev->ev;
    if (ev->is_read_active())
    {
        res = bev->read_in(); // -1 means read max
        if (res > 0)
        {
            bev->add_read_event();
            if (bev->readcb)
                (*bev->readcb)();
        }
        else
        {
            if (res == 0)
            {
                ev->err = EOF;
                if (bev->eofcb)
                    (*bev->eofcb)();
            }
            if (res == -1)
            {
                ev->err = errno;
                if (errno == EAGAIN || errno == EINTR)
                    bev->add_read_event();
                else if (bev->errorcb)
                    (*bev->errorcb)();
            }
        }
    }

    if (ev->is_write_active() && bev->get_obuf_length() > 0)
    {
        res = bev->write_out();
        if (res > 0)
            bev->add_write_event();
        else
        {
            if (res == 0)
                ev->err = EOF;
            if (res == -1)
            {
                if (errno == EAGAIN || errno == EINTR || errno == EINPROGRESS)
                    bev->add_write_event();
                ev->err = errno;
            }
            if (bev->errorcb)
                (*bev->errorcb)();
        }
        if (bev->writecb)
            (*bev->writecb)();
    }
}

} // namespace eve
