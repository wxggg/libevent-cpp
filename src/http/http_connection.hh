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

	std::queue<std::shared_ptr<http_request>> requests;

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

	void close()
	{
		if (get_obuf_length() > 0)
		{
			start_write();
			return;
		}
		clean_event();
		closefd(fd());
		set_fd(-1);
		state = CLOSED;
	}
	inline bool is_closed()
	{
		return state == CLOSED;
	}

	virtual void do_read_done() = 0;
	virtual void do_write_done() = 0;

	void start_read();
	void start_write();

	void remove_read_timer();
	void remove_write_timer();

  protected:
	void read_http();
	void read_firstline(std::shared_ptr<http_request> req);
	void read_header(std::shared_ptr<http_request> req);
	void get_body(std::shared_ptr<http_request> req);
	void read_body(std::shared_ptr<http_request> req);
	void read_trailer(std::shared_ptr<http_request> req);

  private:
	static void handler_read(http_connection *conn);
	static void handler_eof(http_connection *conn);
	static void handler_write(http_connection *conn);
	static void handler_error(http_connection *conn);
};

} // namespace eve
