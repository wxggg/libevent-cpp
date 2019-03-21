#pragma once

#include <http_connection.hh>

namespace eve
{

class http_server;
class http_server_connection : public http_connection, public std::enable_shared_from_this<http_server_connection>
{
public:
  std::weak_ptr<http_server> server;

  std::string clientaddress;
  unsigned int clientport;

public:
  http_server_connection(std::shared_ptr<event_base> base, int fd, std::shared_ptr<http_server> server);
  ~http_server_connection() {}

  auto get_server()
  {
    auto sserver = server.lock();
    if (!sserver)
      LOG_ERROR << " server is expired\n";
    return sserver;
  }

  void fail(http_connection_error error);
  void do_read_done();
  void do_write_done();

  int associate_new_request();
  void handle_request(std::shared_ptr<http_request> req);
};

} // namespace eve
