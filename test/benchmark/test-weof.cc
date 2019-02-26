#include <epoll_base.hh>
#include <poll_base.hh>
#include <rw_event.hh>

#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <iostream>
using namespace std;
using namespace eve;

int fdpair[2];
int called = 0;

void write_cb(rw_event *ev)
{
    cout << __func__ << " called\n";
    const char *test = "test string";
    int len = write(ev->fd, test, strlen(test) + 1);

    cout << "len = " << len << endl;
    if (len > 0)
    {
        if (!called)
            ev->add();
        close(fdpair[0]);
    }
    else if (called == 1)
    {
        cout << "test ok!\n";
    }
    called++;
}

int main(int argc, char const *argv[])
{
    if (signal(SIGPIPE, SIG_IGN) == SIG_IGN)
        return 1;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fdpair) == -1)
        return 1;

    auto base = std::make_shared<poll_base>();

    rw_event ev(base, fdpair[1], WRITE);
    ev.set_callback(write_cb, &ev);
    ev.add();

    base->loop();

    return 0;
}
