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
    std::cout << __func__ << std::endl;
    for (int i = 0; i < threads.size(); i++)
    {
        threads[i]->terminate();
        threads[i]->wakeup();
    }
}

static void loop_task(std::shared_ptr<http_server_thread> thread)
{
    thread->loop();
}

void http_server::resize_thread_pool(int nThreads)
{
    pool->resize(nThreads);
    int currentThreads = threads.size();
    if (currentThreads <= nThreads)
    {
        for (int i = currentThreads; i < nThreads; i++)
        {
            auto thread = std::make_shared<http_server_thread>(shared_from_this());
            threads.push_back(thread);
        }
    }
    else
        threads.resize(nThreads);

    for (int i = 0; i < nThreads; i++)
    {
        pool->push(loop_task, threads[i]);
    }
}



static void __listen_cb(int fd, http_server *server)
{
    std::string host;
    int port;
    int nfd = accept_socket(fd, host, port);
    LOG << "[server] ===> new client in with fd=" << nfd << " hostname=" << host << " portname=" << port << "\n";

    server->clientQueue.push(std::make_shared<http_client_info>(nfd, host, port));

    // auto conn = server->get_empty_connection();
    // conn->set_fd(nfd);
    // conn->clientaddress = host;
    // conn->clientport = port;

    // server->connectionQueue.push(conn);
    server->wakeup_random(2);
}
/*
 * Start a web server on the specified address and port.
 */
int http_server::start(const std::string &address, unsigned short port)
{
    if (threads.size() == 0)
        resize_thread_pool(4);

    int fd = bind_socket(address, port, 1 /*reuse*/);
    if (fd == -1)
        exit(-1);

    if (listenfd(fd) == -1)
        return -1;

    /* use a read event to listen on the fd */
    auto ev = create_event<rw_event>(base, fd, READ);
    base->register_callback(ev, __listen_cb, fd, this);
    ev->set_persistent();
    base->add_event(ev);

    LOG << "[server] Listening on fd = " << fd;
    LOG << "[server] Bound to port " << port << " - Awaiting connections ...";

    base->loop();
    return 0;
}

void http_server::wakeup(int i)
{
    if (i > threads.size())
        return;
    threads[i]->wakeup();
}

/** private function */

} // namespace eve
