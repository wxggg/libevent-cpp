#include "event.hh"

namespace eve
{
class rw_event : public event
{
  private:
	bool _read = false;
	bool _write = false;

	bool _active_read = false;
	bool _active_write = false;

  public:
	int fd = -1;

  public:
	rw_event(event_base *base);
	~rw_event() { std::cout << __func__ << std::endl; }

	void set_fd(int fd) { this->fd = fd; }

	void set_read() { _read = true; }
	void set_write() { _write = true; }
	void clear_read() { _read = false; }
	void clear_write() { _write = false; }

	void set_active_read() { _active_read = true; }
	void set_active_write() { _active_write = true; }
	void clear_active_read() { _active_read = false; }
	void clear_active_write() { _active_write = false; }

	bool is_readable() { return _read; }
	bool is_writable() { return _write; }

	bool has_active_read() { return _active_read; }
	bool has_active_write() { return _active_write; }

	void add();
	void del();
};

} // namespace eve
