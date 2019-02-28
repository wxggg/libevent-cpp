#pragma once

#include <http_request.hh>
#include <buffer_event.hh>
#include <time_event.hh>
#include <util_linux.hh>

#include <queue>
#include <string>
#include <memory>
#include <functional>

namespace eve
{

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

enum http_connection_type
{
	SERVER_CONNECTION,
	CLIENT_CONNECTION
};

class http_connection : public buffer_event
{
  public:
	int timeout = -1;
	enum http_connection_state state;
	enum http_connection_type type;

	std::queue<std::shared_ptr<http_request>> requests;

	time_event *read_timer = nullptr;
	time_event *write_timer = nullptr;

	std::function<void(http_connection *)> closecb = nullptr;
	std::function<void(http_connection *)> connectioncb = nullptr;

  public:
	http_connection(std::shared_ptr<event_base>base);
	virtual ~http_connection();

	inline bool is_server_connection() const { return type == SERVER_CONNECTION; }
	inline bool is_client_connection() const { return type == CLIENT_CONNECTION; }

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

	inline void close()
	{
		closefd(fd); // close tcp connecton
		state = CLOSED;
		del();
	}
	inline bool is_closed() const { return state == CLOSED; }

	virtual void do_read_done() = 0;
	virtual void do_write_active() = 0;
	virtual void do_write_over() = 0;

	inline void start_read()
	{
		if (is_closed())
			return;
		this->add_read();
		state = READING_FIRSTLINE;
		if (timeout > 0)
		{
			read_timer->set_timer(timeout, 0);
			read_timer->add();
		}
	}
	inline void start_write()
	{
		if (is_closed())
			return;
		this->add_write();
		state = WRITING;
		if (timeout > 0)
		{
			write_timer->set_timer(timeout, 0);
			write_timer->add();
		}
	}

  protected:
	void read_http();
	void read_firstline(std::shared_ptr<http_request> req);
	void read_header(std::shared_ptr<http_request> req);
	void get_body(std::shared_ptr<http_request> req);
	void read_body(std::shared_ptr<http_request> req);
	void read_trailer(std::shared_ptr<http_request> req);

  private:
	static void __http_connection_event_cb(http_connection *conn);
};

} // namespace eve
