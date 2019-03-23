#include <http_server_thread.hh>
#include <http_server.hh>

namespace eve
{

void http_server_thread::loop()
{
    base->loop();
}

void http_server_thread::terminate()
{
    base->set_terminated();
}

static int i = 0;
std::shared_ptr<http_server_connection> http_server_thread::get_empty_connection()
{
    if (emptyQueue.empty())
    {
        std::cout << "i=" << i++ << "\n";
        return std::make_shared<http_server_connection>(base, -1, get_server());
    }
    auto conn = emptyQueue.front();
    emptyQueue.pop();
    return conn;
}

void http_server_thread::get_connections(std::shared_ptr<rw_event> ev, http_server_thread *thread)
{
    read_wake_msg(ev->fd);
    auto server = thread->get_server();

    auto i = thread->connectionList.begin();
    while (i != thread->connectionList.end())
    {
        bool isclosed = (*i)->is_closed();
        if (isclosed)
        {
            auto conn = *i;
            i = thread->connectionList.erase(i);
            conn->reset();
            thread->emptyQueue.push(conn);
            LOG_DEBUG << "release empty connection";
        }
        else
            i++;
    }

    std::shared_ptr<http_client_info> cinfo;
    while (server->clientQueue.pop(cinfo))
    {
        auto conn = thread->get_empty_connection();
        conn->set_fd(cinfo->nfd);
        conn->clientaddress = cinfo->host;
        conn->clientport = cinfo->port;

        if (conn->associate_new_request() != -1)
            thread->connectionList.push_back(conn);
    }
}

} // namespace eve
