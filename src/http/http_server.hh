#include <http_server_connection.hh>

#include <list>
#include <string>
#include <map>
#include <memory>

namespace eve
{

typedef void (*handle_cb_t)(std::shared_ptr<http_request>);

class rw_event;
class http_server
{
public:
  event_base *base = NULL;
  int timeout = -1;
  void (*gencb)(std::shared_ptr<http_request>) = NULL;

  std::list<rw_event *> sockets;
  // std::list<struct http_cb *> callbacks;
  std::map<std::string, handle_cb_t> handle_callbacks;
  std::list<std::shared_ptr<http_server_connection>> connections;

  std::string address;
  int port;

public:
  http_server(event_base *base) { this->base = base; }
  ~http_server();

  inline void set_handle_cb(std::string what, handle_cb_t cb)
  {
    handle_callbacks[what] = cb;
  }

  inline void set_timeout(int sec) { timeout = sec; }

  int start(const std::string &address, unsigned short port);

  void clean_connections();
  void get_request(int fd, const std::string &host, int port);

private:
};

} // namespace eve
