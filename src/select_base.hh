#include "event.hh"
#include <vector>
#include <sys/select.h>
#include <signal.h>

namespace eve
{
    
class select_base: public event_base
{
    private:
    	int event_fds;		/* Highest fd in fd set */
    	int event_fdsz;
        
        std::vector<fd_set> event_readset_in;
        std::vector<fd_set> event_readset_out;
        std::vector<fd_set> event_writeset_in;
        std::vector<fd_set> event_writeset_out;

        std::vector<event *> event_r_by_fd;
        std::vector<event *> event_w_by_fd;

    public:
        select_base();
        ~select_base();

        int add(event *ev);
        int del(event *ev);
        int recalc(int max);
        int dispatch(struct timeval *tv);
    private:
        int resize(int fdsz);
        void init();
        int add();
};

} // eve
