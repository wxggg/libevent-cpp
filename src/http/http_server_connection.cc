#include <http_server_connection.hh>
#include <http_server.hh>
#include <util_network.hh>

#include <assert.h>
#include <sys/socket.h>

#include <algorithm>

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

http_server_connection::http_server_connection(std::shared_ptr<event_base> base, int fd, std::shared_ptr<http_server> server)
    : http_connection(base, fd), server(server)
{
    base->register_callback(readTimer, read_timeout_cb, this);
    base->register_callback(writeTimer, write_timeout_cb, this);
    timeout = server->timeout;
}

void http_server_connection::fail(http_connection_error error)
{
    std::cout << __func__ << " state = " << state << "\n";
    std::cout << "<" << std::this_thread::get_id() << ">:" << __func__ << " handle error = " << error << std::endl;
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
        close();
        return;
    case HTTP_INVALID_HEADER:
    default: /* xxx: probably should just error on default */
             /* the callback looks at the uri to determine errors */
        auto req = requests.front();
        if (!req)
            return;
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
    // std::cout << __func__ << " state = " << state << "\n";
    assert(state != CLOSED);

    auto req = requests.front();
    if (req->handled)
    {
        fail(HTTP_EOF);
        return;
    }
    // std::cout << "<" << std::this_thread::get_id() << ">:" << __func__ << " read request uri=" << req->uri << " type=" << req->type << std::endl;

    start_write();
    handle_request(req);
    req->handled = true;
}

int http_server_connection::associate_new_request()
{
    // std::cout << __func__ << " state = " << state << "\n";
    assert(state != CLOSED);

    auto req = std::make_shared<http_request>(shared_from_this());
    req->flags |= REQ_OWN_CONNECTION;

    requests.push(req);

    req->kind = REQUEST;
    req->remote_host = clientaddress;
    req->remote_port = clientport;

    // std::cout << "<" << std::this_thread::get_id() << ">:" << __func__ << " get request from " << clientaddress << ":" << clientport << std::endl;

    start_read();

    return 0;
}

void http_server_connection::handle_request(std::shared_ptr<http_request> req)
{
    // std::cout << __func__ << " state = " << state << "\n";
    assert(state != CLOSED);

    if (req->uri.empty())
    {
        req->send_error(HTTP_BADREQUEST, "Bad Request");
        return;
    }

    std::cout<<"handle:"<<req->uri<<std::endl;
    req->uri = string_from_utf8(req->uri);
    size_t offset = req->uri.find("?");
    if (offset != std::string::npos)
    {
        req->query = req->uri.substr(offset);
        req->uri = req->uri.substr(0, offset);
    }

    auto server = get_server();
    if (server->handle_callbacks.count(req->uri) > 0)
    {
        server->handle_callbacks.at(req->uri)(req);
        return;
    }

    for (const auto &kv : server->handle_callbacks)
    {
        auto v1 = split(kv.first, '/');
        auto v2 = split(req->uri, '/');
        if (v1.size() != v2.size())
            continue;
        bool flag = true;
        for (int i = 0; i < v1.size(); i++)
            if (v1[i] != v2[i] && v1[i] != "*")
            {
                flag = false;
                break;
            }
        if (flag)
        {
            kv.second(req);
            return;
        }
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

void http_server_connection::do_write_done()
{
    // std::cout << __func__ << " state = " << state << "\n";
    assert(state != CLOSED);

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
