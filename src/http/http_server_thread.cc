#include <http_server_thread.hh>
#include <http_server.hh>
#include <signal_event.hh>

namespace eve
{

static void ev_sigpipe_handler(std::shared_ptr<signal_event> ev)
{
    LOG_WARN << " sigpipe on event:" << ev->id;
}

http_server_thread::http_server_thread(http_server *server)
    : server(server)
{
    base = std::make_shared<epoll_base>();

    waker = create_event<rw_event>(base, create_eventfd(), READ);
    waker->set_persistent();
    base->register_callback(waker, get_connections, waker, this);
    base->add_event(waker);

    ev_sigpipe = create_event<signal_event>(base, SIGPIPE);
    ev_sigpipe->set_persistent();
    base->register_callback(ev_sigpipe, ev_sigpipe_handler, ev_sigpipe);
    base->add_event(ev_sigpipe);
}

void http_server_thread::loop()
{
    base->loop();
}

void http_server_thread::terminate()
{
    base->set_terminated();
}

static int i = 0;
std::unique_ptr<http_server_connection> http_server_thread::get_empty_connection()
{
    if (emptyQueue.empty())
    {
        std::cout << "i=" << i++ << "\n";
        return std::unique_ptr<http_server_connection>(new http_server_connection(base, -1, server));
    }

    auto conn = std::move(emptyQueue.front());
    emptyQueue.pop();
    return conn;
}

void http_server_thread::get_connections(std::shared_ptr<rw_event> ev, http_server_thread *thread)
{
    read_wake_msg(ev->fd);

    auto i = thread->connectionList.begin();
    while (i != thread->connectionList.end())
    {
        bool isclosed = (*i)->is_closed();
        if (isclosed)
        {
            auto conn = std::move(*i);
            i = thread->connectionList.erase(i);
            conn->reset();
            thread->emptyQueue.push(std::move(conn));
            LOG_DEBUG << "release empty connection";
        }
        else
            i++;
    }

    std::unique_ptr<http_client_info> cinfo;
    while (thread->server->clientQueue.pop(cinfo))
    {
        auto conn = thread->get_empty_connection();
        conn->set_fd(cinfo->nfd);
        conn->clientaddress = cinfo->host;
        conn->clientport = cinfo->port;

        if (conn->associate_new_request() != -1)
            thread->connectionList.push_back(std::move(conn));
        else
            thread->emptyQueue.push(std::move(conn));
    }
}

} // namespace eve
