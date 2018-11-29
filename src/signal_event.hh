#include "event.hh"

namespace eve
{
class signal_event : public event
{
  public:
    int _sig = -1;

  public:
    signal_event(event_base *base);
    ~signal_event() { std::cout << __func__ << std::endl; }

    void set_sig(int sig) { _sig = sig; }

    void add();
    void del();
};

} // namespace eve
