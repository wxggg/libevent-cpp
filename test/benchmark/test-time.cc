#include <epoll_base.hh>
#include <time_event.hh>
#include <stdlib.h>

#include <iostream>
using namespace std;
using namespace eve;

#define NEVENT 20000
time_event *tmevs[NEVENT];

void time_cb(time_event *ev)
{
    cout << "id=" << ev->id << "  timeout=" << ev->timeout.tv_sec << endl;
}

int main(int argc, char const *argv[])
{
    auto base = std::make_shared<epoll_base>();
    base->priority_init(1);

    time_event *ev = nullptr;
    for (int i = 0; i < NEVENT; i++)
    {
        ev = new time_event(base);
        ev->set_callback(time_cb, ev);
        ev->set_timer(random() % 20, 0);
        ev->add();
    }

    base->loop();

    return 0;
}
