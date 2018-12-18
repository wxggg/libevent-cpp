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

void write_cb(event *argev)
{
    cout << __func__ << " called\n";
    rw_event *ev = (rw_event *)argev;
    char *test = "test string";
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

    // epoll_base base;
    poll_base base;

    rw_event *ev = new rw_event(&base);
    ev->set(fdpair[1], WRITE, write_cb);
    ev->add();

    base.loop();

    return 0;
}
