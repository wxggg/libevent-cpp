#include <http_client_connection.hh>
#include <http_client.hh>
#include <util_network.hh>
#include <util_linux.hh>
#include <event_base.hh>

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
    auto req = current_request();
    if (!req)
        return;
    LOG_ERROR << " req->uri=" << req->uri << " with error=" << error;

    /* xxx: maybe we should fail all requests??? */
    pop_req();

    /* reset the connection */
    this->reset();

    /* We are trying the next request that was queued on us */
    if (!requests.empty())
        this->connect();
}

void http_client_connection::do_read_done()
{
    remove_read_event();
    pop_req();

    if (!requests.empty())
    {
        if (output->get_length() > 0)
            start_write();
        else
        {
            start_read();
            requests.front()->kind = RESPONSE;
        }
    }

    return;
}

void http_client_connection::do_write_done()
{
    auto req = current_request();
    if (!req)
        return;
    start_read();

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

int http_client_connection::make_request(std::unique_ptr<http_request> req)
{
    req->conn = this;
    req->kind = REQUEST;

    req->remote_host = servaddr;
    req->remote_port = servport;

    req->make_header();

    requests.push(std::move(req));
    if (!is_connected())
    {
        if (this->connect() == -1)
            return -1;
    }

    start_write();
    return 0;
}

} // namespace eve
