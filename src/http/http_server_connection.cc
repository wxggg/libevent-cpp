#include <http_server_connection.hh>
#include <http_server.hh>
#include <util_network.hh>

namespace eve
{

static void read_timeout_cb(http_server_connection *conn)
{
    std::cout << __func__ << " called\n";
    conn->fail(HTTP_TIMEOUT);
}

static void write_timeout_cb(http_server_connection *conn)
{
    std::cout << __func__ << " called\n";
    conn->fail(HTTP_TIMEOUT);
}

http_server_connection::http_server_connection(std::shared_ptr<event_base> base, std::shared_ptr<http_server> server)
    : http_connection(base)
{
    this->server = server;
    this->timeout = server->timeout;
    read_timer->set_callback(read_timeout_cb, this);
    write_timer->set_callback(write_timeout_cb, this);

    this->timeout = server->timeout;
    this->type = SERVER_CONNECTION;
}

http_server_connection::~http_server_connection()
{
}

void http_server_connection::fail(http_connection_error error)
{
    auto req = requests.front();
    std::cerr << "[server:FAIL] " << __func__ << " req->uri=" << req->uri << " with error=" << error << std::endl;

    /* 
     * for incoming requests, there are two different
     * failure cases.  it's either a network level error
     * or an http layer error. for problems on the network
     * layer like timeouts we just drop the connections.
     * For HTTP problems, we might have to send back a
     * reply before the connection can be freed.
     */

    switch (error)
    {
    case HTTP_TIMEOUT:
    case HTTP_EOF:
        /* 
         * these are cases in which we probably should just
         * close the connection and not send a reply.  this
         * case may happen when a browser keeps a persistent
         * connection open and we timeout on the read.
         */
        return;
    case HTTP_INVALID_HEADER:
    default: /* xxx: probably should just error on default */
             /* the callback looks at the uri to determine errors */
        if (!req->uri.empty())
            req->uri = "";
        /* 
         * the callback needs to send a reply, once the reply has
         * been send, the connection should get freed.
         */
        if (req->cb)
            req->cb(req);
        handle_request(req);
        break;
    }
}

void http_server_connection::do_read_done()
{
    if (is_closed())
        return;
    del_read();
    auto req = requests.front();
    if (req->handled)
    {
        fail(HTTP_EOF);
        return;
    }
    // std::cout << "[server] done read request uri=" << req->uri << " type=" << req->type << std::endl;

    start_write();
    handle_request(req);
    req->handled = true;
}

int http_server_connection::associate_new_request()
{
    if (is_closed())
        return -1;
    auto req = std::make_shared<http_request>();
    req->conn = this;
    req->flags |= REQ_OWN_CONNECTION;

    requests.push(req);

    req->kind = REQUEST;
    req->remote_host = clientaddress;
    req->remote_port = clientport;

    start_read();

    return 0;
}

void http_server_connection::handle_request(std::shared_ptr<http_request> req)
{
    if (is_closed())
        return;
    if (req->uri.empty())
    {
        req->send_error(HTTP_BADREQUEST, "Bad Request");
        return;
    }

    std::string realuri = req->uri;
    size_t offset = req->uri.find("?");
    if (offset != std::string::npos)
        realuri = req->uri.substr(0, offset);

    if (server->handle_callbacks.count(realuri) > 0)
    {
        server->handle_callbacks.at(realuri)(req);
        return;
    }

    /* generic callback */
    if (server->gencb)
    {
        server->gencb(req);
        return;
    }
    else
        req->send_not_found();
}

void http_server_connection::do_write_over()
{
    if (is_closed())
        return;
    auto req = requests.front();
    if (req->chunked == 1)
        return;
    requests.pop();

    // std::cerr << __func__ << " fixme: deelte possible detection events\n";

    int need_close = (req->minor == 0 && !req->is_connection_keepalive()) ||
                     req->is_connection_close();

    if (need_close)
    {
        close();
        return;
    }

    /* we have a persistent connection; try to accept another request. */
    if (associate_new_request() == -1)
        close();
}

} // namespace eve
