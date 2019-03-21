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

std::shared_ptr<event_base> pbase;

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
void simple_read_cb(std::shared_ptr<rw_event> ev)
{
    cout << __func__ << endl;
    char buf[256];
    int len = read(ev->fd, buf, sizeof(buf));
    cout << "read len = " << len << endl;
    if (len < 0)
        cout << "errno=" << errno << endl;

    if (len)
    {
        if (!called)
        {
            ev->enable_read();
            pbase->add_event(ev);
        }
    }
    else if (called == 1)
        test_ok = 1;

    called++;
}

void simple_write_cb(std::shared_ptr<rw_event> ev)
{
    const char *t = "this is a test";
    int len = write(ev->fd, t, strlen(t) + 1);
    cout << "write len=" << len << endl;
    if (len == -1)
        test_ok = 0;
    else
        test_ok = 1;
}

void test1(void)
{
    const char *t1 = "Simple read :";
    setup_test(t1);

    write(fdpair[0], t1, strlen(t1) + 1);
    shutdown(fdpair[0], SHUT_WR);

    auto ev = create_event<rw_event>(pbase, fdpair[1], READ);
    pbase->register_callback(ev, simple_read_cb, ev);
    pbase->add_event(ev);

    pbase->loop();
    cleanup_test();
}

void test2(void)
{
    setup_test("Simple write: ");

    auto ev = create_event<rw_event>(pbase, fdpair[0], WRITE);
    pbase->register_callback(ev, simple_write_cb, ev);
    pbase->add_event(ev);
    pbase->loop();

    cleanup_test();
}
/**************************************** test 3 4 ******************************/
static char rbuf[4096];
static char wbuf[4096];
static int roff;
static int woff;
static int usepersist;

void multiple_write_cb(std::shared_ptr<rw_event> ev)
{
    int len = 512;
    if (woff + len >= static_cast<int>(sizeof(wbuf)))
        len = sizeof(wbuf) - woff;

    len = write(ev->fd, wbuf + woff, len);
    cout << "len=" << len << endl;

    if (len == -1)
    {
        cerr << __func__ << " : write\n";
        ev->disable_write();
        if (usepersist)
            pbase->remove_event(ev);
        return;
    }
    woff += len;
    if (woff >= static_cast<int>(sizeof(wbuf)))
    {
        shutdown(ev->fd, SHUT_WR);
        ev->disable_write();
        if (usepersist)
            pbase->remove_event(ev);
        return;
    }
    if (!usepersist)
    {
        ev->enable_write();
        pbase->add_event(ev);
    }
}

void multiple_read_cb(std::shared_ptr<rw_event> ev)
{
    int len = read(ev->fd, rbuf + roff, sizeof(rbuf) - roff);
    if (len == -1)
        cerr << __func__ << " : read errno=" << errno << endl;
    if (len <= 0)
    {
        ev->disable_read();
        if (usepersist)
            pbase->remove_event(ev);
        return;
    }
    roff += len;
    if (!usepersist)
    {
        ev->enable_read();
        pbase->add_event(ev);
    }
}

void test3(void)
{
    setup_test("Multiple read/write: ");

    auto rev = create_event<rw_event>(pbase, fdpair[0], READ);
    auto wev = create_event<rw_event>(pbase, fdpair[1], WRITE);

    memset(rbuf, 0, sizeof(rbuf));
    for (int i = 0; i < static_cast<int>(sizeof(wbuf)); i++)
        wbuf[i] = i % 32 + 32;

    roff = woff = 0;
    usepersist = 0;

    pbase->register_callback(wev, multiple_write_cb, wev);
    pbase->register_callback(rev, multiple_read_cb, rev);

    pbase->add_event(wev);
    pbase->add_event(rev);
    pbase->loop();

    if (roff == woff)
        test_ok = memcmp(rbuf, wbuf, sizeof(wbuf)) == 0;

    cleanup_test();
}

void test4(void)
{
    setup_test("Persist read/write: ");

    auto rev = create_event<rw_event>(pbase, fdpair[0], READ);
    auto wev = create_event<rw_event>(pbase, fdpair[1], WRITE);

    memset(rbuf, 0, sizeof(rbuf));
    for (int i = 0; i < static_cast<int>(sizeof(wbuf)); i++)
        wbuf[i] = i % 32 + 32;

    roff = woff = 0;
    usepersist = 1;

    pbase->register_callback(wev, multiple_write_cb, wev);
    pbase->register_callback(rev, multiple_read_cb, rev);

    wev->set_persistent();
    rev->set_persistent();

    pbase->add_event(wev);
    pbase->add_event(rev);

    pbase->loop();

    if (roff == woff)
        test_ok = memcmp(rbuf, wbuf, sizeof(wbuf)) == 0;

    cleanup_test();
}

/**************************************** test 5 ******************************/

void combined_rw_cb(std::shared_ptr<rw_event> ev, int *pnr, int *pnw)
{
    char rbuf[2048], wbuf[2048];

    int rlen, wlen;
    if (ev->is_read_active())
    {
        rlen = read(ev->fd, rbuf, sizeof(rbuf));
        cout << "read len=" << rlen << endl;
        if (rlen > 0)
        {
            *pnr += rlen;
            ev->enable_read();
            pbase->add_event(ev);
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

        cout << "write len=" << wlen << endl;
        if (wlen > 0)
        {
            *pnw -= wlen;
            ev->enable_write();
            pbase->add_event(ev);
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
    setup_test("Combined read/write: ");

    auto rw1 = create_event<rw_event>(pbase, fdpair[0], RDWR);
    auto rw2 = create_event<rw_event>(pbase, fdpair[1], RDWR);

    int nreadr1 = 0, nreadr2 = 0;
    int nreadw1 = 4096, nreadw2 = 8192;

    pbase->register_callback(rw1, combined_rw_cb, rw1, &nreadr1, &nreadw1);
    pbase->register_callback(rw2, combined_rw_cb, rw2, &nreadr2, &nreadw2);

    pbase->add_event(rw1);
    pbase->add_event(rw2);

    pbase->loop();

    if (nreadr1 == 8192 && nreadr2 == 4096)
        test_ok = 1;

    cleanup_test();
}

/**************************************** test 6 7 8 - time out ******************************/
#define SECONDS 1

static struct timeval tset;
static struct timeval tcalled;

void timeout_cb(std::shared_ptr<time_event> ev)
{
    struct timeval tv;

    gettimeofday(&tcalled, nullptr);
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
    setup_test("Simple timeout: ");

    auto ev = create_event<time_event>(pbase);
    ev->set_timer(SECONDS, 0);
    pbase->register_callback(ev, timeout_cb, ev);
    pbase->add_event(ev);

    gettimeofday(&tset, nullptr);
    pbase->loop();

    cleanup_test();
}

void signal_cb(std::shared_ptr<signal_event> ev)
{
    pbase->remove_event(ev);
    test_ok = 1;
}

void test7(void)
{
    setup_test("Simple signal: ");

    auto ev = create_event<signal_event>(pbase, SIGALRM);
    struct itimerval itv;

    pbase->register_callback(ev, signal_cb, ev);
    pbase->add_event(ev);

    memset(&itv, 0, sizeof(itv));
    itv.it_value.tv_sec = 1;
    if (setitimer(ITIMER_REAL, &itv, nullptr) == -1)
        goto skip_simplesignal;

    pbase->loop();
skip_simplesignal:
    pbase->remove_event(ev);

    cleanup_test();
}

void loopexit_cb()
{
    pbase->set_terminated();
}

void test8(void)
{
    auto ev = create_event<time_event>(pbase);
    struct timeval tv, tv_start, tv_end;

    setup_test("Loop exit: ");

    ev->set_timer(60 * 60 * 24, 0);
    pbase->register_callback(ev, timeout_cb, ev);
    pbase->add_event(ev);

    auto loopexit = create_event<time_event>(pbase);
    loopexit->set_timer(1, 0);
    pbase->register_callback(loopexit, loopexit_cb);
    pbase->add_event(loopexit);

    gettimeofday(&tv_start, nullptr);
    pbase->loop();
    gettimeofday(&tv_end, nullptr);

    timersub(&tv_end, &tv_start, &tv);

    pbase->remove_event(ev);

    if (tv.tv_sec < 2)
        test_ok = 1;

    cleanup_test();
}

/**************************************** test 9 10 - buffer event ******************************/

void readcb(buffer_event *bev)
{
    if (bev->get_ibuf_length() == 8333)
    {
        bev->remove_read_event();
        test_ok++;
    }
}

void writecb(buffer_event *bev)
{
    if (bev->get_obuf_length() == 0)
        test_ok++;
}

void errorcb()
{
    test_ok = -2;
}

void test9(void)
{
    setup_test("Bufferevent:  ");

    auto bev1 = std::make_shared<buffer_event>(pbase, fdpair[0]);
    auto bev2 = std::make_shared<buffer_event>(pbase, fdpair[1]);

    bev1->register_writecb(writecb, bev1.get());
    bev2->register_readcb(readcb, bev2.get());

    char buffer[8333];
    for (int i = 0; i < static_cast<int>(sizeof(buffer)); i++)
        buffer[i] = i + 1;

    bev2->add_read_event();

    bev1->write(buffer, sizeof(buffer));

    pbase->loop();

    if (bev2->get_ibuf_length() == 8333)
        test_ok = 1;

    cleanup_test();
}

void test10_readcb(buffer_event *bev, int target)
{
    cout<<"read cb\n";
    if (bev->get_ibuf_length() == target)
        bev->remove_read_event();
}

void test10(void)
{
    setup_test("Combined Bufferevent:  ");

    auto bev1 = std::make_shared<buffer_event>(pbase, fdpair[0]);
    auto bev2 = std::make_shared<buffer_event>(pbase, fdpair[1]);

    int target1 = 8192, target2 = 4096;
    char bufw1[target2], bufw2[target1];

    bev1->register_readcb(test10_readcb, bev1.get(), target1);
    bev2->register_readcb(test10_readcb, bev2.get(), target2);

    bev1->add_read_event();
    bev2->add_read_event();

    bev1->write(bufw1, sizeof(bufw1));
    bev2->write(bufw2, sizeof(bufw2));

    pbase->loop();

    if (bev1->get_ibuf_length() == target1 && bev2->get_ibuf_length() == target2)
        test_ok = 1;

    cleanup_test();
}

/**************************************** test priroties ******************************/

void test_priorities_cb(std::shared_ptr<time_event> ev, int *count)
{
    if (*count == 3)
    {
        pbase->set_terminated();
        return;
    }
    (*count)++;

    ev->set_timer(0, 0);
    pbase->add_event(ev);
}

void test_priorities(int npriorities)
{

    setup_test("Priorities: ");

    cout << "npriorities = " << npriorities << endl;
    pbase->priority_init(npriorities);

    int count1 = 0, count2 = 0;

    auto tev1 = create_event<time_event>(pbase);
    auto tev2 = create_event<time_event>(pbase);

    pbase->register_callback(tev1, test_priorities_cb, tev1, &count1);
    tev1->set_timer(0, 0);
    tev1->set_priority(0);

    pbase->register_callback(tev2, test_priorities_cb, tev2, &count2);
    tev2->set_timer(0, 0);
    tev2->set_priority(npriorities - 1);

    pbase->add_event(tev1);
    pbase->add_event(tev2);

    pbase->loop();

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
    pbase = std::make_shared<epoll_base>();
    // pbase = std::make_shared<poll_base>();
    // pbase = std::make_shared<select_base>();

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

    return 0;
}
