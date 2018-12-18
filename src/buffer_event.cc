#include "buffer_event.hh"

namespace eve
{

buffer_event::buffer_event(event_base *base)
    : rw_event(base)
{
    this->callback = buffer_event_cb;
    this->readcb = this->writecb = this->errorcb = default_cb;
}

size_t buffer_event::write(void *data, size_t size)
{
    // std::cout << __PRETTY_FUNCTION__ << std::endl;
    int res = obuf.push_back(data, size);

    if (res == -1)
        return res;

    if (size > 0 && is_writable())
        this->add();
}

size_t buffer_event::read(void *data, size_t size)
{
    // std::cout << __PRETTY_FUNCTION__ << std::endl;
    return ibuf.pop_front(data, size);
}

void buffer_event::buffer_event_cb(event *argev)
{
    // std::cout << __PRETTY_FUNCTION__ << " called\n";
    buffer_event *ev = (buffer_event *)argev;
    int res = 0;

    if (ev->is_read_active())
    {
        res = ev->ibuf.readfd(ev->fd, -1); // -1 means read max

        if (res > 0)
        {
            ev->add_read();
            (*ev->readcb)(ev);
        }
        else
        {
            (*ev->errorcb)(ev);
            if (res == 0)
                ev->err = EOF;
            if (res == -1)
            {
                if (errno == EAGAIN || errno == EINTR)
                    ev->add_read();
                ev->err = errno;
            }
        }
    }

    if (ev->is_write_active() && ev->get_obuf_length() > 0)
    {
        res = ev->obuf.writefd(ev->fd);
        if (res > 0)
        {
            if (ev->get_obuf_length() > 0)
                ev->add_write();
            (*ev->writecb)(ev);
        }
        else
        {
            (*ev->errorcb)(ev);
            if (res == 0)
                ev->err = EOF;
            if (res == -1)
            {
                if (errno == EAGAIN || errno == EINTR || errno == EINPROGRESS)
                    ev->add_write();
                ev->err = errno;
            }
        }
    }
}

void buffer_event::default_cb(buffer_event *ev)
{
    std::cerr << "error: defualt event buffer callback called\n";
}

} // namespace eve
