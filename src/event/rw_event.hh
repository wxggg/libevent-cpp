#pragma once

#include "event.hh"

namespace eve
{

enum TYPE
{
	READ = 1,
	WRITE,
	RDWR,
};

class rw_event : public event
{
  private:
	bool _read = false;
	bool _write = false;

	bool _read_enabled = true;
	bool _write_enabled = true;

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
	rw_event(std::shared_ptr<event_base> base) : event(base) {}
	rw_event(std::shared_ptr<event_base> base, int fd, TYPE t) : event(base), fd(fd) { set_type(t); }
	~rw_event();

	inline void set_fd(int fd) { this->fd = fd; }
	inline void set_timeout(int sec) { this->timeout = sec; }

	inline void set_read() { _read = true; }
	inline void set_write() { _write = true; }
	inline void clear_read() { _read = false; }
	inline void clear_write() { _write = false; }

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

	inline void set_active_read() { _active_read = true; }
	inline void set_active_write() { _active_write = true; }
	inline void clear_active() { _active_read = _active_write = false; }

	inline bool is_readable() const { return _read; }
	inline bool is_writable() const { return _write; }
	inline bool is_read_write() const { return _read && _write; }

	inline bool is_read_available() const { return !is_read_write() ? _read : _read && _read_enabled; }
	inline bool is_write_available() const { return !is_read_write() ? _write : _write && _write_enabled; }

	inline bool is_removeable() const { return !is_read_write() ? true : (!is_read_available() && !is_write_available()); }

	inline bool is_read_active() const { return _active_read; }
	inline bool is_write_active() const { return _active_write; }

	int add();
	int del();

	void activate_read();
	void activate_write();

	int add_read();
	int add_write();
	int del_read();
	int del_write();
};

} // namespace eve
