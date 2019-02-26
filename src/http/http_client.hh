#include <http_client_connection.hh>

#include <string>
#include <memory>

namespace eve
{

class http_client : public std::enable_shared_from_this<http_client>
{
public:
  int timeout = -1;
  std::shared_ptr<event_base> base;
  std::map<int, std::shared_ptr<http_client_connection>> connections; // map id and connection

public:
  http_client(std::shared_ptr<event_base> base);
  ~http_client();

  /** return id of connection from 0 1 2...
     *  error if return -1 */
  int make_connection(const std::string &address, unsigned int port);
  inline std::shared_ptr<http_client_connection> get_connection(int connid) { return connections[connid]; }

  int make_request(int connid, std::shared_ptr<http_request> req);
};

} // namespace eve
