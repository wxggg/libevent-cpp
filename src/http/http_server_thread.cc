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

void http_server_thread::get_connections(std::shared_ptr<rw_event> ev, http_server_thread *thread)
{
    read_wake_msg(ev->fd);

    thread->connectionList.remove_if([](std::shared_ptr<http_server_connection> conn) { return conn->is_closed(); });

    auto server = thread->get_server();
    std::shared_ptr<http_client_info> cinfo;
    while (server->clientQueue.pop(cinfo))
    {
        auto conn = std::make_shared<http_server_connection>(thread->base, cinfo->nfd, server);
        conn->clientaddress = cinfo->host;
        conn->clientport = cinfo->port;

        if (conn->associate_new_request() != -1)
            thread->connectionList.push_back(conn);
    }
}

} // namespace eve
