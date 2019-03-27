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
  http_server *server;
  std::shared_ptr<rw_event> waker;
  std::shared_ptr<signal_event> ev_sigpipe;
  std::list<std::unique_ptr<http_server_connection>> connectionList;
  std::queue<std::unique_ptr<http_server_connection>> emptyQueue;

public:
  http_server_thread(http_server *server);
  ~http_server_thread()
  {
    base->clean_rw_event(waker);
  }

  void loop();
  void wakeup() { wake(waker->fd); }
  void terminate();

private:
  std::unique_ptr<http_server_connection> get_empty_connection();
  static void get_connections(std::shared_ptr<rw_event> ev, http_server_thread *thread);
};

} // namespace eve
