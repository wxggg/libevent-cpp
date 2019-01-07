#include <http_server.hh>
#include <http_server_connection.hh>
#include <util_network.hh>

#include <string>

namespace eve
{

http_server::~http_server()
{
    for (auto ev : sockets)
    {
        ev->del();
        delete ev;
    }
}

static void __listen_cb(event *argev)
{
    rw_event *ev = (rw_event *)argev;
    http_server *server = (http_server *)ev->data;

    std::string host;
    int port;
    int nfd = accept_socket(ev->fd, host, port);
    std::cout << "[server] ===> new client in with fd=" << nfd << " hostname="
              << host << " portname=" << port << std::endl;
    server->get_request(nfd, host, port);
}
/*
 * Start a web server on the specified address and port.
 */
int http_server::start(const std::string &address, unsigned short port)
{
    int fd = bind_socket(address, port, 1 /*reuse*/);
    if (fd == -1)
        return -1;
    if (listenfd(fd) == -1)
        return -1;

    /* use a read event to listen on the fd */
    rw_event *ev = new rw_event(base);
    ev->data = (void *)this;
    ev->set(fd, READ, __listen_cb);
    ev->set_persistent();
    if (ev->add() == -1)
        return -1;

    sockets.push_back(ev);

    std::cout << "[server] Listening on fd = " << fd << std::endl;
    std::cout << "[server] Bound to port " << port << " - Awaiting connections ..." << std::endl;

    return 0;
}

void http_server::clean_connections()
{
    connections.remove_if([](std::shared_ptr<http_server_connection> conn) { return conn->is_closed(); });
}

void http_server::get_request(int fd, const std::string &host, int port)
{
    std::shared_ptr<http_server_connection> conn(new http_server_connection(this->base, this));
    conn->fd = fd;
    conn->clientaddress = host;
    conn->clientport = port;
    conn->state = READING_FIRSTLINE;

    if (this->timeout > 0)
        conn->set_timeout(this->timeout);

    /* 
	 * if we want to accept more than one request on a connection,
	 * we need to know which http server it belongs to.
	 */
    connections.push_back(conn);

    if (conn->associate_new_request() == -1)
        conn->close();
}

/** private function */

} // namespace eve
