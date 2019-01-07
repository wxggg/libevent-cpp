#pragma once

#include <http_connection.hh>

namespace eve
{

class http_server;
class http_server_connection : public http_connection
{
  public:
    http_server *server = NULL;

    std::string clientaddress;
    unsigned int clientport;

  public:
    http_server_connection(event_base *base, http_server *server);
    ~http_server_connection();

    void fail(http_connection_error error);
    void do_read_done();
    void do_write_active(){}
    void do_write_over();

    int associate_new_request();
    void handle_request(std::shared_ptr<http_request> req);
};

} // namespace eve
