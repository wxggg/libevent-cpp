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

static int count, writes, fired;

static int num_pipes, num_active, num_writes;

static int *pipes;
static vector<rw_event *> vecrw;

event_base *pbase;

void read_cb(event *argev)
{
    // cout << __func__ << " called ";
    rw_event *ev = (rw_event *)argev;
    int idx = (size_t)ev->data, widx = idx + 1;
    // cout << "idx=" << idx;
    u_char ch;

    count += read(ev->fd, &ch, sizeof(ch));
    if (writes)
    {
        if (widx >= num_pipes)
            widx -= num_pipes;
        write(pipes[2 * widx + 1], "e", 1);
        writes--;
        fired++;
    }
    // cout << " count=" << count << " writes=" << writes << " fired=" << fired << endl
    //      << endl;
}

struct timeval *
run_once(void)
{
    rw_event *ev;
    for (int *cp = pipes, i = 0; i < num_pipes; i++, cp += 2)
    {
        ev = vecrw[i];
        ev->del();
        ev->set(cp[0], READ, read_cb);
        ev->set_persistent();
        ev->data = (void *)i;
        ev->add();
    }
    pbase->set_loop_nonblock_and_once();
    pbase->loop();
    pbase->clear_loop_nonblock();

    fired = 0;

    int space = num_pipes / num_active;
    space *= 2;
    for (int i = 0; i < num_active; i++, fired++)
        write(pipes[i * space + 1], "e", 1);

    count = 0;
    writes = num_writes;

    static struct timeval ts, te;
    int xcount = 0;
    gettimeofday(&ts, NULL);
    pbase->set_loop_nonblock_and_once();
    do
    {
        pbase->loop();
        xcount++;
        cout << "xcount=" << xcount << " count=" << count << " fired=" << fired << "--------------" << endl
             << endl;
    } while (count != fired);
    pbase->clear_loop_nonblock();
    gettimeofday(&te, NULL);

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

    cout << "num_pipes:" << num_pipes << " active:" << num_active << " nwrites:" << num_writes << endl;

    if (num_active < 2)
    {
        cerr << "err num_active should not be less then 2\n";
        exit(-1);
    }
    struct rlimit rl;
    rl.rlim_cur = rl.rlim_max = num_pipes * 2 + 50;
    if (setrlimit(RLIMIT_NOFILE, &rl) == -1)
    {
        cerr << "setrlimit\n";
        exit(1);
    }

    pbase = new epoll_base();
    // pbase = new select_base(); // max file descriptor is limited
    // pbase = new poll_base();
    pbase->priority_init(1);

    vecrw.resize(num_pipes);
    for (int i = 0; i < num_pipes; i++)
    {
        vecrw[i] = new rw_event(pbase);
    }
    pipes = new int[num_pipes * 2];
    cout << "memory alloc finished\n";

    for (int *cp = pipes, i = 0; i < num_pipes; i++, cp += 2)
    {
        if (pipe(cp) == -1)
        {
            cerr << "pipe\n";
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
        cout << "测试时间：" << (tv->tv_sec * 1000000L + tv->tv_usec) / 1000 << "ms" << endl;
    }

    return 0;
}
