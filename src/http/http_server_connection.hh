#pragma once

#include <http_connection.hh>

namespace eve
{

class http_server;
class http_server_connection : public http_connection
{
public:
  http_server *server;

  std::string clientaddress;
  unsigned int clientport;

public:
  http_server_connection(std::shared_ptr<event_base> base, int fd, http_server* server);
  ~http_server_connection() {}

  void fail(http_connection_error error);
  void do_read_done();
  void do_write_done();

  int associate_new_request();
  void handle_request(http_request * req);
};

} // namespace eve
