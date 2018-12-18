#include <epoll_base.hh>
#include <select_base.hh>
#include <poll_base.hh>
#include <rw_event.hh>
#include <time_event.hh>
#include <signal_event.hh>
#include <buffer_event.hh>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

#include <utility>

using namespace eve;
using namespace std;

event_base *pbase;

static int fdpair[2];
static int test_ok = 0;
static int called = 0;

int setup_test(const char *name)
{
    cout << name << endl;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fdpair) == -1)
    {
        cerr << __func__ << " socketpair\n";
        exit(1);
    }
    // if (fcntl(fdpair[0], F_SETFL, O_NONBLOCK) == -1)
    //     cerr << "fcntl(O_NONBLOCK)\n";
    // if (fcntl(fdpair[1], F_SETFL, O_NONBLOCK) == -1)
    //     cerr << "fcntl(O_NONBLOCK)\n";

    test_ok = 0;
    called = 0;
    return 0;
}

int cleanup_test(void)
{
    close(fdpair[0]);
    close(fdpair[1]);

    if (test_ok)
        cout << "OK\n";
    else
    {
        cerr << "FAILED\n";
        exit(1);
    }
    return 0;
}
/************************************ test 1 2 ***********************/
void simple_read_cb(event *argev)
{
    rw_event *ev = (rw_event *)argev;

    char buf[256];
    int len = read(ev->fd, buf, sizeof(buf));

    if (len < 0)
        cout << "errno=" << errno << endl;

    if (len)
    {
        if (!called)
            ev->add();
    }
    else if (called == 1)
        test_ok = 1;

    called++;
}

void simple_write_cb(event *argev)
{
    rw_event *ev = (rw_event *)argev;
    const char *t = "this is a test";
    int len = write(ev->fd, t, strlen(t) + 1);
    if (len == -1)
        test_ok = 0;
    else
        test_ok = 1;
}

void test1(void)
{
    rw_event ev(pbase);
    const char *t1 = "Simple read :";
    setup_test(t1);

    write(fdpair[0], t1, strlen(t1) + 1);
    shutdown(fdpair[0], SHUT_WR);

    ev.set(fdpair[1], READ, simple_read_cb);
    ev.add();

    pbase->loop();

    cleanup_test();
}

void test2(void)
{
    rw_event ev(pbase);
    setup_test("Simple write: ");

    ev.set(fdpair[0], WRITE, simple_write_cb);
    ev.add();
    pbase->loop();

    cleanup_test();
}
/**************************************** test 3 4 ******************************/
static char rbuf[4096];
static char wbuf[4096];
static int roff;
static int woff;
static int usepersist;

void multiple_write_cb(event *argev)
{
    // cout << __func__ << endl;
    rw_event *ev = (rw_event *)argev;

    int len = 128;
    if (woff + len >= sizeof(wbuf))
        len = sizeof(wbuf) - woff;

    len = write(ev->fd, wbuf + woff, len);

    if (len == -1)
    {
        cerr << __func__ << " : write\n";
        if (usepersist)
            ev->del();
        return;
    }
    woff += len;
    if (woff >= sizeof(wbuf))
    {
        shutdown(ev->fd, SHUT_WR);
        if (usepersist)
            ev->del();
        return;
    }
    if (!usepersist)
        ev->add();
}

void multiple_read_cb(event *argev)
{
    // cout << __func__ << endl;
    rw_event *ev = (rw_event *)argev;

    int len = read(ev->fd, rbuf + roff, sizeof(rbuf) - roff);
    if (len == -1)
        cerr << __func__ << " : read errno=" << errno << endl;
    if (len <= 0)
    {
        if (usepersist)
            ev->del();
        return;
    }
    roff += len;
    if (!usepersist)
        ev->add();
}

void test3(void)
{
    rw_event rev(pbase), wev(pbase);

    setup_test("Multiple read/write: ");

    memset(rbuf, 0, sizeof(rbuf));
    for (int i = 0; i < sizeof(wbuf); i++)
        wbuf[i] = i;

    roff = woff = 0;
    usepersist = 0;

    wev.set(fdpair[0], WRITE, multiple_write_cb);
    rev.set(fdpair[1], READ, multiple_read_cb);

    rev.add();
    wev.add();

    pbase->loop();

    if (roff == woff)
        test_ok = memcmp(rbuf, wbuf, sizeof(wbuf)) == 0;

    cleanup_test();
}

void test4(void)
{
    rw_event rev(pbase), wev(pbase);

    setup_test("Persist read/write: ");

    memset(rbuf, 0, sizeof(rbuf));
    for (int i = 0; i < sizeof(wbuf); i++)
        wbuf[i] = i;

    roff = woff = 0;
    usepersist = 1;

    wev.set(fdpair[0], WRITE, multiple_write_cb);
    wev.set_persistent();

    rev.set(fdpair[1], READ, multiple_read_cb);
    rev.set_persistent();

    rev.add();
    wev.add();

    pbase->loop();

    if (roff == woff)
        test_ok = memcmp(rbuf, wbuf, sizeof(wbuf)) == 0;

    cleanup_test();
}

/**************************************** test 5 ******************************/

void combined_rw_cb(event *argev)
{
    rw_event *ev = (rw_event *)argev;
    int *pnr = (int *)ev->rdata, *pnw = (int *)ev->wdata;
    char rbuf[4096], wbuf[4096];

    int rlen, wlen;
    if (ev->is_read_active())
    {
        rlen = read(ev->fd, rbuf, sizeof(rbuf));

        if (rlen > 0)
        {
            *pnr += rlen;
            ev->add();
        }
        else
        {
            if (rlen == -1)
                cerr << __func__ << " :read\n";
        }
    }

    if (ev->is_write_active())
    {
        wlen = sizeof(wbuf);
        if (wlen > *pnw)
            wlen = *pnw;
        if (wlen > 0)
            wlen = write(ev->fd, wbuf, wlen);

        if (wlen > 0)
        {
            *pnw -= wlen;
            ev->add();
        }
        else
        {
            if (wlen == -1)
                cerr << __func__ << " :write\n";
            shutdown(ev->fd, SHUT_WR); // shutdown write
        }
    }
}

void test5(void)
{
    rw_event rw1(pbase), rw2(pbase);
    setup_test("Combined read/write: ");

    int nreadr1 = 0, nreadr2 = 0;
    int nreadw1 = 4096, nreadw2 = 8192;

    rw1.rdata = &nreadr1;
    rw1.wdata = &nreadw1;
    rw2.rdata = &nreadr2;
    rw2.wdata = &nreadw2;

    rw1.set(fdpair[0], RDWR, combined_rw_cb);
    rw2.set(fdpair[1], RDWR, combined_rw_cb);

    rw1.add();
    rw2.add();

    pbase->loop();

    if (nreadr1 == 8192 && nreadr2 == 4096)
        test_ok = 1;

    cleanup_test();
}

/**************************************** test 6 7 8 - time out ******************************/
#define SECONDS 1

static struct timeval tset;
static struct timeval tcalled;

void timeout_cb(event *argev)
{
    rw_event *ev = (rw_event *)argev;
    struct timeval tv;

    gettimeofday(&tcalled, NULL);
    if (timercmp(&tcalled, &tset, >))
        timersub(&tcalled, &tset, &tv);
    else
        timersub(&tset, &tcalled, &tv);

    int diff = tv.tv_sec * 1000 + tv.tv_usec / 1000 - SECONDS * 1000;
    if (diff < 0)
        diff = -diff;

    if (diff < 100)
        test_ok = 1;
}

void test6(void)
{
    time_event ev(pbase);
    struct timeval tv;

    setup_test("Simple timeout: ");

    ev.set_timer(SECONDS, 0);
    ev.set_callback(timeout_cb);
    ev.add();

    gettimeofday(&tset, NULL);
    pbase->loop();

    cleanup_test();
}

void signal_cb(event *argev)
{
    signal_event *ev = (signal_event *)argev;
    ev->del();
    test_ok = 1;
}

void test7(void)
{
    signal_event ev(pbase);
    struct itimerval itv;

    setup_test("Simple signal: ");
    ev.set(SIGALRM, signal_cb);
    ev.add();

    memset(&itv, 0, sizeof(itv));
    itv.it_value.tv_sec = 1;
    if (setitimer(ITIMER_REAL, &itv, NULL) == -1)
        goto skip_simplesignal;

    pbase->loop();
skip_simplesignal:
    ev.del();

    cleanup_test();
}

void loopexit_cb(event *argev)
{
    pbase->set_terminated(true);
}

void test8(void)
{
    time_event ev(pbase);
    struct timeval tv, tv_start, tv_end;

    setup_test("Loop exit: ");

    ev.set_timer(60 * 60 * 24, 0);
    ev.set_callback(timeout_cb);
    ev.add();

    time_event loopexit(pbase);
    loopexit.set_timer(1, 0);
    loopexit.set_callback(loopexit_cb);
    loopexit.add();

    gettimeofday(&tv_start, NULL);
    pbase->loop();
    gettimeofday(&tv_end, NULL);

    timersub(&tv_end, &tv_start, &tv);

    ev.del();

    if (tv.tv_sec < 2)
        test_ok = 1;

    cleanup_test();
}

/**************************************** test 9 10 - buffer event ******************************/

void readcb(buffer_event *bev)
{
    if (bev->get_ibuf_length() == 8333)
    {
        bev->del_read();
        test_ok++;
    }
}

void writecb(buffer_event *bev)
{
    if (bev->get_obuf_length() == 0)
        test_ok++;
}

void errorcb(buffer_event *bev)
{
    test_ok = -2;
}

void test9(void)
{
    buffer_event bev1(pbase), bev2(pbase);
    setup_test("Bufferevent:  ");

    bev1.set_fd(fdpair[0]);
    bev1.set_write();
    bev1.set_cb(0, writecb, errorcb);

    bev2.set_fd(fdpair[1]);
    bev2.set_read();
    bev2.set_cb(readcb, 0, errorcb);

    char buffer[8333];
    for (int i = 0; i < sizeof(buffer); i++)
        buffer[i] = i + 1;

    bev2.add();

    bev1.write(buffer, sizeof(buffer));

    pbase->loop();

    if (test_ok != 2)
        test_ok = 0;

    cleanup_test();
}

void test10_readcb(buffer_event *bev)
{
    if (bev->get_ibuf_length() == *(int *)bev->data)
        bev->del_read();
}

void wcb(buffer_event *bev) {}
void ecb(buffer_event *bev)
{
    cout << "error ecb called\n";
}

void test10(void)
{
    buffer_event bev1(pbase), bev2(pbase);
    setup_test("Combined Bufferevent:  ");

    bev1.set_fd(fdpair[0]);
    bev1.set_read();
    bev1.set_write();
    bev1.set_cb(test10_readcb, wcb, ecb);

    bev2.set_fd(fdpair[1]);
    bev2.set_read();
    bev2.set_write();
    bev2.set_cb(test10_readcb, wcb, ecb);

    char bufw1[4096], bufw2[8192];
    bev1.write(bufw1, sizeof(bufw1));
    bev2.write(bufw2, sizeof(bufw2));

    int target1 = 8192, target2 = 4096;
    bev1.data = &target1;
    bev2.data = &target2;

    pbase->loop();

    // cout << bev1.get_ibuf_length() << "  " << bev2.get_ibuf_length() << endl;
    // cout << bev1.get_obuf_length() << "  " << bev2.get_obuf_length() << endl;

    if (bev1.get_ibuf_length() == target1 && bev2.get_ibuf_length() == target2)
        test_ok = 1;

    cleanup_test();
}

/**************************************** test priroties ******************************/

void test_priorities_cb(event *argev)
{
    // cout << __func__ << endl;
    time_event *ev = (time_event *)argev;
    int *count = (int *)ev->data;
    if (*count == 3)
    {
        pbase->set_terminated(true);
        return;
    }

    (*count)++;

    ev->set_timer(0, 0);
    ev->add();
}

void test_priorities(int npriorities)
{

    setup_test("Priorities: ");

    cout << "npriorities = " << npriorities << endl;
    pbase->priority_init(npriorities);

    int count1 = 0, count2 = 0;

    time_event tev1(pbase), tev2(pbase);
    tev1.set_callback(test_priorities_cb);
    tev1.set_timer(0, 0);
    tev1.set_priority(0);
    tev1.data = &count1;

    tev2.set_callback(test_priorities_cb);
    tev2.set_timer(0, 0);
    tev2.set_priority(npriorities - 1);
    tev2.data = &count2;

    tev1.add();
    tev2.add();

    pbase->loop();

    tev1.del();
    tev2.del();

    if (npriorities == 1)
    {
        if (count1 == 3 && count2 == 3)
            test_ok = 1;
    }
    else
    {
        if (count1 == 3 && count2 == 0)
            test_ok = 1;
    }

    cleanup_test();
}


int main(int argc, char const *argv[])
{
    // pbase = new epoll_base();
    // pbase = new select_base();
    pbase = new poll_base();

    test1();

    test2();

    test3();

    test4();

    test5();

    test6();

    test7();

    test8();

    test9();

    test10();

    test_priorities(1);
    test_priorities(2);
    test_priorities(3);

    delete pbase;

    return 0;
}
