#include <iostream>
#include <event.hh>
#include <select_base.hh>
#include <signal.h>

using namespace std;
using namespace eve;

int called = 0;

void signal_cb(int fd, short event, void *arg)
{
    cout<<"signal call back \n";
    eve::event *ev = (eve::event*) arg;
    ev->ev_base->del_event(ev);
}

int main(int argc, char const *argv[])
{
    eve::select_base sel;
    sel.priority_init(1); // allocate a single active queue

    eve::event signal_int;
    signal_int.set(&sel, SIGINT, EV_SIGNAL|EV_PERSIST, &signal_cb);

    sel.add_event(&signal_int, NULL);

    sel.loop(0);

    return 0;
}
