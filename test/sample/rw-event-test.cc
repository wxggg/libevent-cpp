#include <rw_event.hh>
#include <select_base.hh>
#include <signal_event.hh>
#include <poll_base.hh>
#include <epoll_base.hh>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

using namespace std;
using namespace eve;

void fifo_read(std::shared_ptr<rw_event> ev)
{
    char buf[255];
    int len = read(ev->fd, buf, sizeof(buf) - 1);
    if (len == -1)
    {
        cerr << "read" << endl;
        return;
    }
    else if (len == 0)
    {
        cerr << "connection closed" << endl;
        return;
    }
    buf[len] = '\0';
    cout << buf << endl;
}

void signal_int(std::shared_ptr<signal_event> ev)
{
    cout << "signal event " << ev->sig << " called back" << endl;
    exit(-1);
}

int main(int argc, char const *argv[])
{
    // auto base = std::make_shared<select_base>();
    auto base = std::make_shared<epoll_base>();
    // auto base = std::make_shared<epoll_base>();

    const char *fifo = "/tmp/event.fifo";

    unlink(fifo);
    if (mkfifo(fifo, 0600) == -1)
    {
        cerr << "mkfifo" << endl;
        exit(1);
    }

    int socket = open(fifo, O_RDWR | O_NONBLOCK, 0);
    cout << "socket=" << socket << endl;

    if (socket == -1)
    {
        cerr << "open" << endl;
        exit(1);
    }

    auto evfifo = create_event<rw_event>(base, socket, READ);
    base->register_callback(evfifo, fifo_read, evfifo);
    base->add_event(evfifo);

    auto evsigint = create_event<signal_event>(base, SIGINT);
    base->register_callback(evsigint, signal_int, evsigint);
    //add

    base->loop();

    return 0;
}
