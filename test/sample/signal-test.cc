#include <iostream>
#include <event.hh>
#include <select_base.hh>
#include <signal.h>

#include <signal_event.hh>

using namespace std;
using namespace eve;

int called = 0;

void signal_cb(event *argev)
{
    cout<<"signal call back \n";
    signal_event *ev = (signal_event*) argev;
    ev->del();
}

int main(int argc, char const *argv[])
{
    select_base base;

    signal_event ev(&base);
    ev.set(SIGINT, signal_cb);
    ev.add();

    base.loop();

    return 0;
}
