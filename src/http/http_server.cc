#include <http_server.hh>
#include <http_server_connection.hh>
#include <util_network.hh>
#include <event_base.hh>
#include <util_linux.hh>

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

void dispatch_connections(rw_event *ev, std::shared_ptr<event_base> base, http_server *server)
{
    char buf[32];
    size_t n = read(ev->fd, buf, sizeof(buf));

    int i = 0;
    http_client_info *cinfo;
    while (server->clientQueue.pop(cinfo))
    {
        auto conn = new http_server_connection(base, server->shared_from_this());
        conn->fd = cinfo->nfd;
        conn->clientaddress = cinfo->host;
        conn->clientport = cinfo->port;
        conn->state = READING_FIRSTLINE;
        if (server->timeout > 0)
            conn->set_timeout(server->timeout);

        if (conn->associate_new_request() == -1)
            conn->close();
    }
}

void http_server::resize_thread_pool(int nThreads)
{
    pool->resize(nThreads);
}

static void loop_task(std::shared_ptr<event_base> base)
{
    base->loop();
}

static void __listen_cb(int fd, http_server *server)
{

    std::string host;
    int port;
    int nfd = accept_socket(fd, host, port);
    // std::cout << "[server] ===> new client in with fd=" << nfd << " hostname="
    //           << host << " portname=" << port << std::endl;
    auto cinfo = new http_client_info(nfd, host, port);
    server->clientQueue.push(cinfo);
    server->set_loops();
    server->wakeup(1);
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
    rw_event *ev = new rw_event(base, fd, READ);
    ev->set_callback(__listen_cb, fd, this);
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
    // connections.remove_if([](std::shared_ptr<http_server_connection> conn) { return conn->is_closed(); });
}

void http_server::set_loops()
{
    int nidle = idle_threads();
    if (nidle > 0)
    {
        for (int i = 0; i < nidle; i++)
        {
            auto _base = std::make_shared<epoll_base>();
            auto ev = new rw_event(_base, create_eventfd(), READ);
            ev->set_persistent();
            ev->set_callback(dispatch_connections, ev, _base, this);
            ev->add();
            wakefds.push_back(ev->fd);
            pool->push(loop_task, _base);
        }
    }
}

void http_server::get_connection(int fd, const std::string &host, int port)
{
}

void http_server::wakeup(int nloops)
{
    for (int i = 0; i < nloops; i++)
    {
        auto randIt = wakefds.begin();
        std::advance(randIt, std::rand() % wakefds.size());
        std::string msg = "0x123456";
        size_t n = write(*randIt, msg.c_str(), msg.length());
    }
}

/** private function */

} // namespace eve
