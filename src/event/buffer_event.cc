#include "buffer_event.hh"

namespace eve
{

buffer_event::buffer_event(event_base *base)
    : rw_event(base)
{
    this->callback = buffer_event_cb;
    this->readcb = this->writecb = this->errorcb = default_cb;

    this->input_buffer = new buffer();
    this->output_buffer = new buffer();
}

buffer_event::~buffer_event()
{
    delete input_buffer;
    delete output_buffer;
}

size_t buffer_event::write(void *data, size_t size)
{
    // std::cout << __PRETTY_FUNCTION__ << std::endl;
    int res = output_buffer->push_back(data, size);

    if (res == -1)
        return -1;

    if (size > 0 && is_writable())
        this->add();
    return res;
}

size_t buffer_event::read(void *data, size_t size)
{
    // std::cout << __PRETTY_FUNCTION__ << std::endl;
    return input_buffer->pop_front(data, size);
}

void buffer_event::buffer_event_cb(event *argev)
{
    // std::cout << __PRETTY_FUNCTION__ << " called\n";
    buffer_event *ev = (buffer_event *)argev;
    int res = 0;

    if (ev->is_read_active())
    {
        res = ev->input_buffer->readfd(ev->fd, -1); // -1 means read max
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
        res = ev->output_buffer->writefd(ev->fd);
        if (res > 0)
        {
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
    std::cerr << "--error: defualt event buffer callback called\n";
    std::cout << "--ev: fd=" << ev->fd << std::endl;
    if (ev->is_read_active())
        std::cout << "read active\n";
    if (ev->is_write_active())
        std::cout << "write active\n";
}

} // namespace eve
