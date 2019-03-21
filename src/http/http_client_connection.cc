#include <http_client_connection.hh>
#include <http_client.hh>
#include <util_network.hh>
#include <util_linux.hh>
#include <event_base.hh>

#include <cassert>

namespace eve
{

static void read_timeout_cb(http_client_connection *conn)
{
    LOG_WARN << "client connection read timeout";
    conn->fail(HTTP_TIMEOUT);
}

static void write_timeout_cb(http_client_connection *conn)
{
    LOG_WARN << "client connection read timeout";
    conn->fail(HTTP_TIMEOUT);
}

http_client_connection::http_client_connection(std::shared_ptr<event_base> base, int fd, std::shared_ptr<http_client> client)
    : http_connection(base, fd), client(client)
{
    base->register_callback(readTimer, read_timeout_cb, this);
    base->register_callback(writeTimer, write_timeout_cb, this);
    this->timeout = client->timeout;
}

void http_client_connection::fail(http_connection_error error)
{
    auto req = requests.front();
    LOG_ERROR << " req->uri=" << req->uri << " with error=" << error;

    this->requests.pop();
    /* xxx: maybe we should fail all requests??? */

    /* reset the connection */
    this->reset();

    /* We are trying the next request that was queued on us */
    if (!requests.empty())
        this->connect();

    if (req->cb)
        req->cb(req);
}

void http_client_connection::do_read_done()
{
    remove_read_event();
    if (requests.empty())
        return;

    auto req = requests.front();
    requests.pop();

    if (req->cb)
        req->cb(req);

    if (!requests.empty())
    {
        auto req = requests.front();
        req->make_header();
        start_write();
    }

    return;
}

void http_client_connection::do_write_done()
{
    if (requests.empty())
        return;

    assert(state == WRITING);

    start_read();

    auto req = requests.front();
    req->kind = RESPONSE;
}

int http_client_connection::connect()
{
    if (state == CONNECTING)
        return 0;

    reset();

    int thefd = fd();
    if (socket_connect(thefd, servaddr, servport) == -1)
    {
        closefd(thefd);
        return -1;
    }
    add_write_event();
    state = CONNECTING;
    return 0;
}

int http_client_connection::make_request(std::shared_ptr<http_request> req)
{
    req->conn = shared_from_this();
    req->kind = REQUEST;

    req->remote_host = servaddr;
    req->remote_port = servport;

    requests.push(req);
    // dispatch();
    if (!is_connected())
    {
        if (this->connect() == -1)
            return -1;
    }

    req->make_header();
    start_write();

    return 0;
}

} // namespace eve
