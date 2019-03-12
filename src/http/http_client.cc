#include <http_client.hh>
#include <util_network.hh>
#include <epoll_base.hh>

#include <cassert>

namespace eve
{
http_client::http_client()
{
    base = std::make_shared<epoll_base>();
}

std::shared_ptr<http_client_connection> http_client::make_connection(const std::string &address, unsigned int port)
{
    int fd = get_nonblock_socket();

    auto conn = std::make_shared<http_client_connection>(base, fd, shared_from_this());
    conn->servaddr = address;
    conn->servport = port;

    if (conn->connect() == -1)
        return nullptr;
    return conn;
}

void http_client::run()
{
    base->loop();
}

} // namespace eve
