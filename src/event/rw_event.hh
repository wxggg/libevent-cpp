#pragma once

#include <event.hh>
#include <util_linux.hh>

namespace eve
{

enum TYPE
{
	NONE = 0,
	READ,
	WRITE,
	RDWR,
};

class rw_event : public event
{
  private:
	bool _read = false;
	bool _write = false;

	bool _active_read = false;
	bool _active_write = false;

  public:
	int fd = -1;
	void *rdata;
	void *wdata;

	bool epoll_in = false;
	bool epoll_out = false;

	int timeout = -1;

  public:
	rw_event() {}
	rw_event(std::shared_ptr<event_base> base) : event(base) {}
	rw_event(std::shared_ptr<event_base> base, int fd, TYPE t) : event(base), fd(fd) { set_type(t); }
	~rw_event()
	{
		if (fd != -1)
			closefd(fd);
	}

	inline void set_fd(int fd) { this->fd = fd; }
	inline void set_timeout(int sec) { this->timeout = sec; }

	inline void enable_read() { _read = true; }
	inline void enable_write() { _write = true; }
	inline void disable_read() { _read = false; }
	inline void disable_write() { _write = false; }

	inline void set_type(TYPE t)
	{
		if (t == READ)
			_read = true;
		else if (t == WRITE)
			_write = true;
		else if (t == RDWR)
			_read = _write = true;
		else
			_read = _write = false;
	}

	inline void set_active_read()
	{
		_active_read = true;
		if (!is_persistent())
			disable_read();
	}

	inline void set_active_write()
	{
		_active_write = true;
		if (!is_persistent())
			disable_write();
	}

	inline void clear_active() { _active_read = _active_write = false; }

	inline bool is_readable() const { return _read; }
	inline bool is_writeable() const { return _write; }
	inline bool is_read_write() const { return _read && _write; }

	inline bool is_removeable() const { return !_read && !_write; }

	inline bool is_read_active() const { return _active_read; }
	inline bool is_write_active() const { return _active_write; }
};

} // namespace eve
