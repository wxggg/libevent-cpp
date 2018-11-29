#include "event.hh"

namespace eve
{

class time_event : public event
{

  public:
    struct timeval _timeout;

  public:
    time_event(event_base *base);
    ~time_event() { std::cout << __func__ << std::endl; }

    void set_timer(int nsec);

    struct timeval *timer_left();

    void add();
    void del();
};

} // namespace eve
