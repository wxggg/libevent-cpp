#include <http_client_connection.hh>
#include <event_base.hh>

#include <string>
#include <memory>

namespace eve
{

class http_client : public std::enable_shared_from_this<http_client>
{
public:
  int timeout = -1;
  std::shared_ptr<event_base> base = nullptr;

public:
  http_client();
  ~http_client() {}

  inline void set_timeout(int sec) { timeout = sec; }

  std::unique_ptr<http_client_connection> make_connection(
      const std::string &address, unsigned int port);

  void run();
};

} // namespace eve
