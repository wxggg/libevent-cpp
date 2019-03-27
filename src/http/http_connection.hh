#pragma once

#include <http_request.hh>
#include <buffer_event.hh>
#include <time_event.hh>
#include <util_linux.hh>

#include <queue>
#include <string>
#include <memory>
#include <functional>
#include <mutex>

namespace eve
{
using Lock = std::unique_lock<std::mutex>;

enum http_connection_error
{
	HTTP_TIMEOUT,
	HTTP_EOF,
	HTTP_INVALID_HEADER
};

enum http_connection_state
{
	DISCONNECTED,	  /**< not currently connected not trying either*/
	CONNECTING,		   /**< tries to currently connect */
	IDLE,			   /**< connection is established */
	READING_FIRSTLINE, /**< reading Request-Line (incoming conn) or
				        **< Status-Line (outgoing conn) */
	READING_HEADERS,   /**< reading request/response headers */
	READING_BODY,	  /**< reading request/response body */
	READING_TRAILER,   /**< reading request/response chunked trailer */
	WRITING,		   /**< writing request/response headers/body */
	CLOSED			   /**< connection closed > */
};

class http_connection : public buffer_event
{
  protected:
	int timeout = -1;

	std::queue<std::unique_ptr<http_request>> requests;
	std::queue<std::unique_ptr<http_request>> emptyQueue;

	std::shared_ptr<time_event> readTimer = nullptr;
	std::shared_ptr<time_event> writeTimer = nullptr;

	std::function<void(http_connection *)> closecb = nullptr;
	std::function<void(http_connection *)> connectioncb = nullptr;

	std::mutex mutex;

  public:
	enum http_connection_state state;
	http_connection(std::shared_ptr<event_base> base, int fd);
	virtual ~http_connection();

	inline int is_connected() const
	{
		switch (state)
		{
		case DISCONNECTED:
		case CONNECTING:
			return 0;
		case IDLE:
		case READING_FIRSTLINE:
		case READING_BODY:
		case READING_TRAILER:
		case WRITING:
		default:
			return 1;
		}
	}

	void reset();

	virtual void fail(enum http_connection_error error) = 0;

	void close(int op);

	inline bool is_closed()
	{
		return state == CLOSED;
	}

	virtual void do_read_done() = 0;
	virtual void do_write_done() = 0;

	void start_read();
	void start_write();

	void add_read_and_timer();
	void add_write_and_timer();

	void remove_read_timer();
	void remove_write_timer();

  protected:
	inline http_request *current_request()
	{
		if (requests.empty())
		{
			LOG_WARN << " no request ";
			close(1);
			return nullptr;
		}
		return requests.front().get();
	}

	inline void pop_req()
	{
		if (requests.empty())
			return;
		auto req = std::move(requests.front());
		requests.pop();

		if (req->cb)
			req->cb(req.get());

		req->reset();
		emptyQueue.push(std::move(req));
	}

	inline decltype(auto) get_empty_request()
	{
		if (emptyQueue.empty())
		{
			return std::make_unique<http_request>(this);
		}
		auto req = std::move(emptyQueue.front());
		emptyQueue.pop();
		return req;
	}

	void read_http();
	void read_firstline();
	void read_header();
	void get_body();
	void read_body();
	void read_trailer();

  private:
	static void handler_read(http_connection *conn);
	static void handler_eof(http_connection *conn);
	static void handler_write(http_connection *conn);
	static void handler_error(http_connection *conn);
};

} // namespace eve
