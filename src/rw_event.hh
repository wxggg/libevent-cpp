#include "event.hh"

namespace eve
{
class rw_event : public event
{
  public:
	int _fd = -1;

  public:
	rw_event(event_base *base);
	~rw_event() { std::cout << __func__ << std::endl; }

	void set_read() { _events |= EV_READ; }
	void set_write() { _events |= EV_WRITE; }
	void set_fd(int fd) { _fd = fd; }

	void add();
	void del();
};


} // namespace eve
