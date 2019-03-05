#include <http_server_connection.hh>
#include <thread_pool.hh>
#include <epoll_base.hh>
#include <lock_queue.hh>

#include <list>
#include <string>
#include <map>
#include <memory>
#include <functional>
#include <vector>

namespace eve
{
using HandleCallBack = std::function<void(std::shared_ptr<http_request>)>;

class rw_event;
class epoll_base;

class http_client_info
{
  public:
	int nfd;
	int port;
	std::string host;

  public:
	http_client_info(int nfd, std::string host, int port) : nfd(nfd), port(port), host(host) {}
};

class http_server : public std::enable_shared_from_this<http_server>
{
  private:
	std::shared_ptr<thread_pool> pool = nullptr;
	std::vector<std::shared_ptr<event_base>> poolBases;
	std::vector<int> wakefds;

  public:
	std::shared_ptr<event_base> base = nullptr;
	int timeout = -1;

	std::function<void(std::shared_ptr<http_request>)> gencb = nullptr;

	std::list<rw_event *> sockets;
	std::map<std::string, HandleCallBack> handle_callbacks;
	lock_queue<std::shared_ptr<http_client_info>> clientQueue;

	std::string address;
	int port;

	std::mutex mutex;

  public:
	http_server(std::shared_ptr<event_base> base)
	{
		this->base = base;
		pool = std::make_shared<thread_pool>();
		resize_thread_pool(4); // default 4 threads
	}
	~http_server();

	void resize_thread_pool(int nThreads);
	inline int idle_threads() { return pool->idle_size(); }

	inline void set_handle_cb(std::string what, HandleCallBack cb)
	{
		handle_callbacks[what] = cb;
	}

	void set_loops();

	inline void set_timeout(int sec) { timeout = sec; }

	int start(const std::string &address, unsigned short port);

	void clean_connections();
	void wakeup(int nloops); // wakeup the ith thread in pool

  private:
};

} // namespace eve
