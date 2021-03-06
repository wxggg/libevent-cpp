#include <http_server_connection.hh>
#include <http_server.hh>
#include <util_network.hh>
#include <logger.hh>

#include <sys/socket.h>

#include <algorithm>

namespace eve
{

static void read_timeout_cb(http_server_connection *conn)
{
    LOG_WARN << "server connection read timeout " << conn->clientaddress << ":" << conn->clientport;
    conn->fail(HTTP_TIMEOUT);
}

static void write_timeout_cb(http_server_connection *conn)
{
    LOG_WARN << "server connection write timeout " << conn->clientaddress << ":" << conn->clientport;
    conn->fail(HTTP_TIMEOUT);
}

http_server_connection::http_server_connection(
    std::shared_ptr<event_base> base, int fd, http_server *server)
    : http_connection(base, fd), server(server)
{
    base->register_callback(readTimer, read_timeout_cb, this);
    base->register_callback(writeTimer, write_timeout_cb, this);
    timeout = server->timeout;
}

void http_server_connection::fail(http_connection_error error)
{
    LOG_WARN << "server connection fail on error=" << error << " state=" << state;
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
        close(1);
        return;
    case HTTP_EOF:
        /* 
         * these are cases in which we probably should just
         * close the connection and not send a reply.  this
         * case may happen when a browser keeps a persistent
         * connection open and we timeout on the read.
         */
        close(0);
        return;
    case HTTP_INVALID_HEADER:
    default: /* xxx: probably should just error on default */
             /* the callback looks at the uri to determine errors */
        auto req = current_request();
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
    auto req = current_request();
    if (!req)
        return;
    if (req->handled)
    {
        fail(HTTP_EOF);
        return;
    }

    start_write();
    handle_request(req);
    req->handled = true;
}

int http_server_connection::associate_new_request()
{
    auto req = get_empty_request();
    req->flags |= REQ_OWN_CONNECTION;

    req->kind = REQUEST;
    req->remote_host = clientaddress;
    req->remote_port = clientport;

    requests.push(std::move(req));

    LOG << "<" << std::this_thread::get_id() << ">:"
        << " get request from " << clientaddress << ":" << clientport;

    start_read();

    return 0;
}

void http_server_connection::handle_request(http_request *req)
{
    if (req->uri.empty())
    {
        req->send_error(HTTP_BADREQUEST, "Bad Request");
        LOG_ERROR << "handle " << HTTP_BADREQUEST << " Bad Request";
        return;
    }

    LOG << "handle uri=" << req->uri;

    req->uri = string_from_utf8(req->uri);
    size_t offset = req->uri.find("?");
    if (offset != std::string::npos)
    {
        req->query = req->uri.substr(offset);
        req->uri = req->uri.substr(0, offset);
    }

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
        for (int i = 0; i < static_cast<int>(v1.size()); i++)
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
    auto req = current_request();
    if (!req)
        return;
    if (req->chunked == 1)
        return;

    bool need_close =req->is_connection_close();

    pop_req();

    if (need_close)
    {
        close(0);
        return;
    }

    /* we have a persistent connection; try to accept another request. */
    if (associate_new_request() == -1)
        close(1);
}

} // namespace eve
