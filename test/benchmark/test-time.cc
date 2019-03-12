#include <epoll_base.hh>
#include <time_event.hh>
#include <stdlib.h>

#include <iostream>
using namespace std;
using namespace eve;

#define NEVENT 2000

void time_cb(std::shared_ptr<time_event> ev)
{
    cout << "id=" << ev->id << "  timeout=" << ev->timeout.tv_sec <<" usec="<<ev->timeout.tv_usec << endl;
    ev->get_base()->clean_event(ev);
}

int main(int argc, char const *argv[])
{
    auto base = std::make_shared<epoll_base>();
    base->priority_init(1);

    for (int i = 0; i < NEVENT; i++)
    {
        auto ev = create_event<time_event>(base);
        ev->set_timer(random() % 20, 0);
        base->register_callback(ev, time_cb, ev);
        base->add_event(ev);
    }


    base->loop();

    return 0;
}
