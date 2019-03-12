#include <iostream>
#include <event.hh>
#include <select_base.hh>
#include <signal.h>

#include <signal_event.hh>

using namespace std;
using namespace eve;

int called = 0;

void signal_cb(std::shared_ptr<signal_event> ev)
{
    cout << "signal call back \n";
    ev->get_base()->remove_event(ev);
}

int main(int argc, char const *argv[])
{
    auto base = std::make_shared<select_base>();

    auto ev = create_event<signal_event>(base, SIGINT);
    base->register_callback(ev, signal_cb, ev);
    base->add_event(ev);

    base->loop();

    return 0;
}
