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

void fifo_read(event *argev)
{
    rw_event *ev = (rw_event *)argev;
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

void signal_int(event *argev)
{
    signal_event *ev  =(signal_event*) argev;
    cout<<"signal event "<<ev->sig<<" called back"<<endl;
    exit(-1);
}

int main(int argc, char const *argv[])
{
    // select_base base;
    poll_base base;
    // epoll_base base;

    rw_event evfifo(&base);

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

    evfifo.set(socket, READ, fifo_read);
    evfifo.add();

    signal_event evsigint(&base);
    evsigint.set(SIGINT, signal_int);
    // evsigint.add();

    base.loop();

    return 0;
}
