#include <http_client_connection.hh>
#include <http_client.hh>
#include <util_network.hh>
#include <util_linux.hh>

#include <cassert>

namespace eve
{

static void read_timeout_cb()
{
    // conn->fail(HTTP_TIMEOUT);
}

static void write_timeout_cb()
{
    // conn->fail(HTTP_TIMEOUT);
}

http_client_connection::http_client_connection(std::shared_ptr<event_base> base, std::shared_ptr<http_client> client)
    : http_connection(base)
{
    this->client = client;
    read_timer->set_callback(read_timeout_cb);
    read_timer->data = (void *)this;
    write_timer->set_callback(write_timeout_cb);
    write_timer->data = (void *)this;

    this->timeout = client->timeout;
    this->type = CLIENT_CONNECTION;
}

http_client_connection::~http_client_connection()
{
}

void http_client_connection::fail(http_connection_error error)
{
    auto req = requests.front();
    std::cerr << "[FAIL] " << __func__ << " req->uri=" << req->uri << " with error=" << error << std::endl;

    this->requests.pop();
    /* xxx: maybe we should fail all requests??? */

    /* reset the connection */
    this->reset();

    /* We are trying the next request that was queued on us */
    if (!requests.empty())
        this->connect();

    if (req->cb)
        req->cb(nullptr);
}

void http_client_connection::do_read_done()
{
    del_read();
    if (requests.empty())
        return;

    auto req = requests.front();
    requests.pop();

    if (req->cb)
        req->cb(req);

    return;
}

void http_client_connection::do_write_active()
{
    // if (check_socket(fd) == -1)
    // {
    //     std::cerr << __func__ << " : fixme !!!\n";
    //     return;
    // }
    retry_cnt = 0;

    // dispatch();
    return;
}

void http_client_connection::do_write_over()
{
    if (requests.empty())
        return;

    assert(state == WRITING);

    this->add_read();
    state = READING_FIRSTLINE;

    auto req = requests.front();
    req->kind = RESPONSE;
}

void http_client_connection::dispatch()
{
    if (requests.empty())
        return;

    if (!is_connected())
    {
        if (this->connect() == -1)
            return;
    }

    auto req = requests.front();

    assert(state == IDLE);

    state = WRITING;
    req->make_header();

    this->add_write();
}

int http_client_connection::connect()
{
    if (state == CONNECTING)
        return 0;

    reset();

    fd = get_nonblock_socket();
    if (fd == -1)
        return -1;

    if (socket_connect(fd, servaddr, servport) == -1)
    {
        closefd(fd);
        fd = -1;
        return -1;
    }
    add();
    state = CONNECTING;
    return 0;
}

} // namespace eve
