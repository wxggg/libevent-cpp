#include "event_base.hh"

#include <vector>
#include <map>
#include <sys/select.h>
#include <signal.h>

namespace eve
{

class rw_event;
class select_base : public event_base
{
  private:
    int event_fds = 0; /* Highest fd in fd set */
    int event_fdsz = 0;

    fd_set *event_readset_in = nullptr;
    fd_set *event_writeset_in = nullptr;

    fd_set *event_readset_out = nullptr;
    fd_set *event_writeset_out = nullptr;

    std::map<int, rw_event *> event_r_by_fd;
    std::map<int, rw_event *> event_w_by_fd;

  public:
    select_base();
    ~select_base();

    int add(rw_event *ev);
    int del(rw_event *ev);
    int recalc(int max);
    int dispatch(struct timeval *tv);

  private:
    int resize(int fdsz);
    void init();
    int add();

    void check_fdset();
};

} // namespace eve
