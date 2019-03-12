#pragma once

#include <http_server_connection.hh>
#include <epoll_base.hh>

#include <list>

namespace eve
{

class http_server_thread
{
  private:
    std::shared_ptr<event_base> base = nullptr;
    std::weak_ptr<http_server> server;
    std::shared_ptr<rw_event> waker;
    std::list<std::shared_ptr<http_server_connection>> connectionList;

  public:
    http_server_thread(std::shared_ptr<http_server> server)
        : server(server)
    {
        base = std::make_shared<epoll_base>();
        waker = create_event<rw_event>(base, create_eventfd(), READ);
        waker->set_persistent();
        base->register_callback(waker, get_connections, waker, this);
        base->add_event(waker);
    }
    ~http_server_thread()
    {
        base->clean_event(waker);
    }

    auto get_server()
    {
        auto s = server.lock();
        if (!s)
            std::cerr << "[Error] server is expired\n";
        return s;
    }

    void loop();
    void wakeup();
    void terminate();

  private:
    static void get_connections(std::shared_ptr<rw_event> ev, http_server_thread *thread);
};

} // namespace eve
