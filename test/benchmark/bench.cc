#include <epoll_base.hh>
#include <select_base.hh>
#include <poll_base.hh>
#include <rw_event.hh>

#include <sys/resource.h>
#include <unistd.h>
#include <assert.h>

#include <vector>

using namespace std;
using namespace eve;

static int num_pipes, num_active, num_writes;

static int *pipes;
vector<std::shared_ptr<rw_event>> vecrw;

std::shared_ptr<event_base> pbase;

void read_cb(int fd, int idx, int *count, int *writes, int *fired)
{
    // cout << "read_cb fd=" << fd << " count=" << *count << " writes=" << *writes << " fired=" << *fired << endl;
    int widx = idx + 1;
    u_char ch;

    (*count) += read(fd, &ch, sizeof(ch));
    if (*writes)
    {
        if (widx >= num_pipes)
            widx -= num_pipes;
        write(pipes[2 * widx + 1], "e", 1);
        (*writes)--;
        (*fired)++;
    }
}

struct timeval *
run_once(void)
{
    static int count = 0, fired = 0;
    int writes = num_writes;
    for (int *cp = pipes, i = 0; i < num_pipes; i++, cp += 2)
    {
        auto ev = vecrw[i];
        pbase->remove_event(ev);
        ev->enable_read();
        ev->set_fd(cp[0]);
        ev->set_persistent();
        pbase->register_callback(ev, read_cb, ev->fd, i, &count, &writes, &fired);
        pbase->add_event(ev);
    }
    pbase->loop_nonblock_and_once();

    int space = num_pipes / num_active;
    space *= 2;
    for (int i = 0; i < num_active; i++, fired++)
        write(pipes[i * space + 1], "e", 1);

    static struct timeval ts, te;
    int xcount = 0;
    gettimeofday(&ts, nullptr);
    do
    {
        pbase->loop_nonblock_and_once();
        xcount++;
        // this_thread::sleep_for(chrono::milliseconds(10));
    } while (count != fired);
    gettimeofday(&te, nullptr);

    if (xcount != count)
        cerr << "Xcount:" << xcount << ", Rcount:" << count << endl;

    timersub(&te, &ts, &te);

    return &te;
}

int main(int argc, char *const argv[])
{
    num_pipes = 100;
    num_active = 2;
    num_writes = num_pipes / 2;

    int c;
    extern char *optarg;
    while ((c = getopt(argc, argv, "n:a:w:")) != -1)
    {
        switch (c)
        {
        case 'n':
            num_pipes = atoi(optarg);
            break;
        case 'a':
            num_active = atoi(optarg);
            break;
        case 'w':
            num_writes = atoi(optarg);
            break;
        default:
            cerr << "illegal argument" << endl;
            exit(1);
        }
    }

    struct rlimit rl;
    rl.rlim_cur = rl.rlim_max = num_pipes * 2 + 50;
    if (setrlimit(RLIMIT_NOFILE, &rl) == -1)
    {
        cerr << "setrlimit\n";
        exit(1);
    }

    pbase = std::make_shared<epoll_base>();
    // pbase = std::make_shared<select_base>(); // max file descriptor is limited
    // pbase = std::make_shared<poll_base>();

    pbase->priority_init(1);

    vecrw.resize(num_pipes);
    for (int i = 0; i < num_pipes; i++)
        vecrw[i] = create_event<rw_event>(pbase, -1, READ);
    pipes = new int[num_pipes * 2];
    cout << "memory alloc finished\n";

    for (int *cp = pipes, i = 0; i < num_pipes; i++, cp += 2)
    {
        if (pipe(cp) == -1)
        {
            cerr << "error pipe errno=" << errno << endl;
            exit(-1);
        }
    }

    cout << "pipes alloc finished\n";

    struct timeval *tv;
    for (int i = 0; i < 25; i++)
    {
        cout << "run_once " << i + 1 << endl;
        tv = run_once();
        if (!tv)
            exit(1);
        cout << "测试时间：" << (tv->tv_sec * 1000000L + tv->tv_usec)<< " microseconds" << endl;
    }

    delete[] pipes;

    return 0;
}
