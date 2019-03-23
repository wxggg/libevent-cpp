#include <http_connection.hh>
#include <http_server.hh>
#include <util_network.hh>
#include <util_linux.hh>

#include <iostream>
#include <cstring>
#include <string>
#include <cassert>

namespace eve
{

/** class http_connection **/

http_connection::http_connection(std::shared_ptr<event_base> base, int fd)
    : buffer_event(base, fd)
{
    this->state = DISCONNECTED;

    register_readcb(handler_read, this);
    register_eofcb(handler_eof, this);
    register_writecb(handler_write, this);
    register_errorcb(handler_error, this);

    readTimer = create_event<time_event>(base);
    writeTimer = create_event<time_event>(base);
}

http_connection::~http_connection()
{
    // std::cout << __func__ << std::endl;
    /* notify interested parties that this connection is going down */
    // if (this->fd != -1)
    // {
    //     if (this->is_connected() && this->closecb != nullptr)
    //         this->closecb(this);
    // }

    auto base = get_base();
    if (base)
    {
        base->remove_event(readTimer);
        base->remove_event(writeTimer);
    }
}

void http_connection::close()
{
    if (get_obuf_length() > 0)
    {
        start_write();
        return;
    }
    LOG << "close connection with fd=" << fd();
    get_base()->remove_event(readTimer);
    get_base()->remove_event(writeTimer);
    get_base()->clean_rw_event(ev);
    closefd(fd());
    set_fd(-1);
    state = CLOSED;
}

void http_connection::reset()
{
    // if (fd() != -1)
    // {
    //     // remove_read_event();
    //     // remove_write_event();
    //     // remove_read_timer();
    //     // remove_write_timer();
    //     // /* inform interested parties about connection close */
    //     // if (is_connected() && closecb != nullptr)
    //     //     closecb(this);
    //     close();
    // }
    state = DISCONNECTED;
    input->reset();
    output->reset();
}

void http_connection::start_read()
{
    assert(state != CLOSED);
    add_read_event();
    state = READING_FIRSTLINE;
    if (timeout > 0)
    {
        readTimer->set_timer(timeout, 0);
        get_base()->add_event(readTimer);
    }
}

void http_connection::start_write()
{
    assert(state != CLOSED);
    if (get_obuf_length() <= 0)
        return;
    add_write_event();
    state = WRITING;
    if (timeout > 0)
    {
        writeTimer->set_timer(timeout, 0);
        get_base()->add_event(writeTimer);
    }
}

void http_connection::remove_read_timer()
{
    get_base()->remove_event(readTimer);
}

void http_connection::remove_write_timer()
{
    get_base()->remove_event(writeTimer);
}

/** private function **/

void http_connection::read_firstline(std::shared_ptr<http_request> req)
{
    enum message_read_status res = req->parse_firstline(input);
    if (res == DATA_CORRUPTED)
    {
        /* Error while reading, terminate */
        fail(HTTP_INVALID_HEADER);
        return;
    }
    else if (res == MORE_DATA_EXPECTED)
    {
        /* Need more header lines */
        add_read_event();
        return;
    }

    this->state = READING_HEADERS;
    read_header(req);
}

void http_connection::read_header(std::shared_ptr<http_request> req)
{
    enum message_read_status res = req->parse_headers(input);
    if (res == DATA_CORRUPTED)
    {
        /* Error while reading, terminate */
        fail(HTTP_INVALID_HEADER);
        return;
    }
    else if (res == MORE_DATA_EXPECTED)
    {
        add_read_event();
        return;
    }

    /* Done reading headers, do the real work */

    switch (req->kind)
    {
    case REQUEST:
        get_body(req);
        break;
    case RESPONSE:
        if (req->response_code == HTTP_NOCONTENT || req->response_code == HTTP_NOTMODIFIED ||
            (req->response_code >= 100 && req->response_code < 200))
        {
            do_read_done();
        }
        else
        {
            get_body(req);
        }
        break;
    default:
        LOG_ERROR << ": bad request kind";
        fail(HTTP_INVALID_HEADER);
        break;
    }
}

void http_connection::get_body(std::shared_ptr<http_request> req)
{
    /* If this is a request without a body, then we are done */
    if (req->kind == REQUEST && req->type != REQ_POST)
    {
        do_read_done();
        return;
    }
    state = READING_BODY;
    if (req->input_headers["Transfer-Encoding"] == "chunked")
    {
        req->chunked = 1;
        req->ntoread = -1;
    }
    else
    {
        if (req->get_body_length() == -1)
        {
            LOG_ERROR << " error get body length = -1";
            fail(HTTP_INVALID_HEADER);
            return;
        }
    }
    read_body(req);
}

void http_connection::read_body(std::shared_ptr<http_request> req)
{
    if (req->chunked > 0)
    {
        switch (req->handle_chunked_read(input))
        {
        case ALL_DATA_READ:
            /* finished last chunk */
            state = READING_TRAILER;
            read_trailer(req);
            return;
        case DATA_CORRUPTED:
            fail(HTTP_INVALID_HEADER);
            return;
        case REQUEST_CANCELED:
            return;
        case MORE_DATA_EXPECTED:
        default:
            break;
        }
    }
    else if (req->ntoread < 0)
    {
        /* Read until connection close. */
        req->input_buffer->push_back_buffer(this->input, -1);
    }
    else if (get_ibuf_length() >= req->ntoread)
    {
        /* Completed content length */
        req->input_buffer->push_back_buffer(this->input, (size_t)req->ntoread);
        req->ntoread = 0;
        do_read_done();
        return;
    }

    /* Read more! */
    // this->add_read();
    add_read_event();
}

void http_connection::read_trailer(std::shared_ptr<http_request> req)
{
    switch (req->parse_headers(input))
    {
    case DATA_CORRUPTED:
        fail(HTTP_INVALID_HEADER);
        break;
    case ALL_DATA_READ:
        do_read_done();
        break;
    case MORE_DATA_EXPECTED:
    default:
        add_read_event();
        break;
    }
}

/** buffer_event callback 
 *  @readcb: dealing with read from fd
 *  @writecb: write to client
 *  @errorcb
 **/

void http_connection::read_http()
{
    // std::cout << __func__ << " state = " << state << "\n";
    if (is_closed() || requests.empty())
        return;
    auto req = requests.front();
    switch (state)
    {
    case READING_FIRSTLINE:
        read_firstline(req);
        break;
    case READING_HEADERS:
        read_header(req);
        break;
    case READING_BODY:
        read_body(req);
        break;
    case READING_TRAILER:
        read_trailer(req);
        break;
    case DISCONNECTED:
    case CONNECTING:
    case IDLE:
    case WRITING:
    default:
        LOG_ERROR << ": illegal connection state " << state;
        break;
    }
}

void http_connection::handler_read(http_connection *conn)
{
    conn->remove_read_timer();
    conn->read_http();
}

void http_connection::handler_eof(http_connection *conn)
{
    LOG << "connection fd=" << conn->fd();
    if (conn->get_obuf_length() > 0)
        conn->start_write();
}

void http_connection::handler_write(http_connection *conn)
{
    conn->remove_write_timer();
    if (conn->get_obuf_length() > 0)
    {
        conn->add_write_event();
    }
    else
    {
        conn->remove_write_event();
        conn->do_write_done();
    }
}

void http_connection::handler_error(http_connection *conn)
{
    LOG_ERROR << "[HTTP] error read/write connection fd=" << conn->fd();
    conn->fail(HTTP_EOF);
}

} // namespace eve
