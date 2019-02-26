#include <iostream>
#include <event.hh>
#include <select_base.hh>
#include <signal.h>

#include <signal_event.hh>

using namespace std;
using namespace eve;

int called = 0;

void signal_cb(signal_event *ev)
{
    cout << "signal call back \n";
    ev->del();
}

int testcb(int i, int j)
{
    cout << "i=" << i << " j=" << j << endl;
    return i + j;
}

int main(int argc, char const *argv[])
{
    auto base = std::make_shared<select_base>();

    signal_event ev(base, SIGINT);
    ev.set_callback(signal_cb, &ev);

    ev.add();

    base->loop();

    return 0;
}
