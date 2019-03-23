#pragma once

#include <http_server_connection.hh>
#include <epoll_base.hh>
#include <util_linux.hh>
#include <logger.hh>

#include <list>
#include <queue>

namespace eve
{

class http_server_thread
{
private:
  std::shared_ptr<event_base> base = nullptr;
  std::weak_ptr<http_server> server;
  std::shared_ptr<rw_event> waker;
  std::list<std::shared_ptr<http_server_connection>> connectionList;
  std::queue<std::shared_ptr<http_server_connection>> emptyQueue;

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
    base->clean_rw_event(waker);
  }

  auto get_server()
  {
    auto s = server.lock();
    if (!s)
      LOG_ERROR << "[Error] server is expired\n";
    return s;
  }

  void loop();
  void wakeup() { wake(waker->fd); }
  void terminate();

private:
  std::shared_ptr<http_server_connection> get_empty_connection();
  static void get_connections(std::shared_ptr<rw_event> ev, http_server_thread *thread);
};

} // namespace eve
