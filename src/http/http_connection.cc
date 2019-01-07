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

http_connection::http_connection(event_base *base)
    : buffer_event(base)
{
    this->state = DISCONNECTED;
    set_type(RDWR);

    this->set_callback(__http_connection_event_cb);

    this->read_timer = new time_event(base);
    this->write_timer = new time_event(base);
}

http_connection::~http_connection()
{
    /* notify interested parties that this connection is going down */
    if (this->fd != -1)
    {
        if (this->is_connected() && this->closecb != NULL)
            (*this->closecb)(this);
    }
    del_read();
    del_write();

    this->read_timer->del();
    this->write_timer->del();

    delete read_timer;
    delete write_timer;
}

void http_connection::reset()
{
    if (fd != -1)
    {
        this->del();
        /* inform interested parties about connection close */
        if (is_connected() && closecb != NULL)
            (*closecb)(this);
        closefd(fd);
        fd = -1;
    }
    state = DISCONNECTED;
    input_buffer->reset();
    output_buffer->reset();
}

/** private function **/

void http_connection::read_firstline(std::shared_ptr<http_request> req)
{
    enum message_read_status res = req->parse_firstline(input_buffer);
    if (res == DATA_CORRUPTED)
    {
        /* Error while reading, terminate */
        std::cerr << "[connect] " << __func__ << ": bad header lines on" << this->fd << std::endl;
        fail(HTTP_INVALID_HEADER);
        return;
    }
    else if (res == MORE_DATA_EXPECTED)
    {
        /* Need more header lines */
        this->add_read();
        return;
    }

    this->state = READING_HEADERS;
    read_header(req);
}

void http_connection::read_header(std::shared_ptr<http_request> req)
{
    enum message_read_status res = req->parse_headers(input_buffer);
    if (res == DATA_CORRUPTED)
    {
        /* Error while reading, terminate */
        std::cerr << "[connect] " << __func__ << ": bad header lines on " << fd << std::endl;
        fail(HTTP_INVALID_HEADER);
        return;
    }
    else if (res == MORE_DATA_EXPECTED)
    {
        this->add_read();
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
            std::cerr << "[connect] " << __func__ << ": skipping body for code "
                      << req->response_code << std::endl;
            do_read_done();
        }
        else
        {
            std::cerr << "[connect] " << __func__ << ": start of read body for " << req->remote_host
                      << " on " << fd << std::endl;
            get_body(req);
        }
        break;
    default:
        std::cerr << "[connect] " << __func__ << ": bad header on " << fd << std::endl;
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
            std::cerr << "[connect] " << __func__ << " error get body length = -1\n";
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
        switch (req->handle_chunked_read(input_buffer))
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
        req->input_buffer->push_back_buffer(this->input_buffer, -1);
    }
    else if (get_ibuf_length() >= req->ntoread)
    {
        /* Completed content length */
        req->input_buffer->push_back_buffer(this->input_buffer, (size_t)req->ntoread);
        req->ntoread = 0;
        do_read_done();
        return;
    }

    /* Read more! */
    this->add_read();
}

void http_connection::read_trailer(std::shared_ptr<http_request> req)
{
    switch (req->parse_headers(input_buffer))
    {
    case DATA_CORRUPTED:
        fail(HTTP_INVALID_HEADER);
        break;
    case ALL_DATA_READ:
        do_read_done();
        break;
    case MORE_DATA_EXPECTED:
    default:
        this->add_read();
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
    if (is_closed() || requests.empty())
        return;
    std::shared_ptr<http_request> req = requests.front();
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
        std::cerr << "[connect] " << __func__ << " with id=" << id << ": illegal connection state " << state << std::endl;
        break;
    }
}

/** callback for active read or write events
 *  called by event_process()
 */
void http_connection::__http_connection_event_cb(event *argev)
{
    http_connection *conn = (http_connection *)argev;
    if (conn->is_closed())
        return;
    int res = 0;

    if (conn->is_read_active())
    {
        res = conn->read_in();
        if (res > 0)
        {
            conn->add_read();
            conn->read_http();
        }
        else
        {
            if (res == 0)
            {
                conn->do_read_done();
                return;
            }
            if (res == -1)
            {
                if (errno == EAGAIN || errno == EINTR)
                    conn->add_read();
                else
                    conn->fail(HTTP_EOF);
                return;
            }
        }
    }

    if (conn->is_write_active())
    {
        conn->do_write_active();
        if (conn->get_obuf_length() > 0)
        {
            res = conn->write_out();
            if (res > 0)
            {
                if (conn->get_obuf_length() > 0)
                    conn->add_write();
                else
                    conn->do_write_over();
            }
            else
            {
                if (res == 0)
                {
                    conn->fail(HTTP_EOF);
                    return;
                }
                if (res == -1)
                {
                    if (errno == EAGAIN || errno == EINTR || errno == EINPROGRESS)
                        conn->add_write();
                    else
                        conn->fail(HTTP_EOF);
                    return;
                }
            }
        }
    }
}

} // namespace eve
