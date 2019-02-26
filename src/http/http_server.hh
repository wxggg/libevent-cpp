#include <http_server_connection.hh>

#include <list>
#include <string>
#include <map>
#include <memory>
#include <functional>

namespace eve
{
using HandleCallBack = std::function<void(std::shared_ptr<http_request>)>;

class rw_event;
class http_server : public std::enable_shared_from_this<http_server>
{
  public:
	std::shared_ptr<event_base> base = nullptr;
	int timeout = -1;

	std::function<void(std::shared_ptr<http_request>)> gencb = nullptr;

	std::list<rw_event *> sockets;
	std::map<std::string, HandleCallBack> handle_callbacks;
	std::list<std::shared_ptr<http_server_connection>> connections;

	std::string address;
	int port;

  public:
	http_server(std::shared_ptr<event_base> base) { this->base = base; }
	~http_server();

	inline void set_handle_cb(std::string what, HandleCallBack cb)
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
