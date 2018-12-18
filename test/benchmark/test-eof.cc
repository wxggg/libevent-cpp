#include <epoll_base.hh>
#include <poll_base.hh>
#include <rw_event.hh>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

using namespace std;
using namespace eve;

int called = 0;
void read_cb(event *argev)
{
    cout << __func__ << " called\n";
    char buf[256];
    rw_event *ev = (rw_event *)argev;
    int len = read(ev->fd, buf, sizeof(buf));

    cout << "read:" << buf << " len=" << len << endl;
    if (len)
        ev->add();
    else
        cout << "EOF" << endl;
}

int main(int argc, char const *argv[])
{
    // epoll_base base;
    poll_base base;
    base.priority_init(1);

    char *test = "test string";
    int pair[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == -1)
        return 1;

    write(pair[0], test, strlen(test) + 1);
    shutdown(pair[0], SHUT_WR); // shutdown write

    rw_event *ev = new rw_event(&base);
    ev->set(pair[1], READ, read_cb);
    cout<<"pari1="<<pair[1]<<endl;

    ev->add();

    base.loop();

    return 0;
}
