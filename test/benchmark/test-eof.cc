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
void read_cb(std::shared_ptr<rw_event> ev)
{
    cout << __func__ << " called\n";
    char buf[256];
    int len = read(ev->fd, buf, sizeof(buf));

    cout << "read:" << buf << " len=" << len << endl;
    if (len)
    {
        ev->enable_read();
        ev->get_base()->add_event(ev);
    }
    else
        cout << "EOF" << endl;
}

int main(int argc, char const *argv[])
{
    auto base = std::make_shared<poll_base>();
    base->priority_init(1);

    const char *test = "test string";
    int pair[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == -1)
        return 1;

    write(pair[0], test, strlen(test) + 1);
    shutdown(pair[0], SHUT_WR); // shutdown write

    auto ev = create_event<rw_event>(base, pair[1], READ);
    base->register_callback(ev, read_cb, ev);

    cout << "pari1=" << pair[1] << endl;

    base->add_event(ev);

    base->loop();

    return 0;
}
