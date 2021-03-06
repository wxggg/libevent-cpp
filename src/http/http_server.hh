#include <http_server_connection.hh>
#include <thread_pool.hh>
#include <epoll_base.hh>
#include <lock_queue.hh>
#include <http_server_thread.hh>

#include <list>
#include <string>
#include <map>
#include <memory>
#include <functional>
#include <vector>

namespace eve
{
using HandleCallBack = std::function<void(http_request *)>;

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

class http_server
{
private:
	std::shared_ptr<thread_pool> pool = nullptr;
	std::shared_ptr<event_base> base = nullptr;
	std::vector<std::unique_ptr<http_server_thread>> threads;

public:
	int timeout = -1;

	lock_queue<std::unique_ptr<http_client_info>> clientQueue;

	std::function<void(http_request *)> gencb = nullptr;

	std::map<std::string, HandleCallBack> handle_callbacks;

	std::string address;
	int port;

	std::mutex mutex;

public:
	http_server()
	{
		base = std::make_shared<epoll_base>();
		pool = std::make_shared<thread_pool>();
	}
	~http_server();

	void resize_thread_pool(int nThreads);
	inline int idle_threads() { return pool->idle_size(); }

	inline void set_handle_cb(std::string what, HandleCallBack cb)
	{
		handle_callbacks[what] = cb;
	}

	inline void set_gen_cb(HandleCallBack cb)
	{
		gencb = cb;
	}

	inline void set_timeout(int sec) { timeout = sec; }

	int start(const std::string &address, unsigned short port);

	void clean_connections();
	void wakeup(int i); // wakeup the ith thread in pool
	void wakeup_random(int n)
	{
		for (int i = 0; i < n; i++)
			wakeup(rand() % static_cast<int>(threads.size()));
	}

	void wakeup_all()
	{
		for (int i = 0; i < static_cast<int>(threads.size()); i++)
			wakeup(i);
	}

private:
};

} // namespace eve
