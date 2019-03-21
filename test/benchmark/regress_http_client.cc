#include <http_server.hh>
#include <epoll_base.hh>
#include <poll_base.hh>
#include <buffer_event.hh>
#include <util_network.hh>
#include <http_client.hh>
#include <time_event.hh>

#include <iostream>
#include <string>
#include <vector>

using namespace std;
using namespace eve;

static int test_ok = 0;
std::string host = "127.0.0.1";
unsigned short port = 8102;

static const string CHUNKS[] = {
    "This is funny",
    "but no hilarious.",
    "bwv 1052"};

static void http_readcb(buffer_event *bev)
{
    cout << __func__ << endl;
    if (bev->get_ibuf()->find_string("This is funny") != nullptr)
    {
        auto req = std::make_shared<http_request>(nullptr);
        req->kind = RESPONSE;
        req->parse_firstline(bev->get_ibuf());
        req->parse_headers(bev->get_ibuf());
        for (const auto &p : req->input_headers)
            cout << p.first << "\t===\t" << p.second << endl;
        bev->remove_read_event();
    }
    else
    {
        cout << "[FAIL] not found\n";
        exit(-1);
    }
}

static void http_writecb(buffer_event *bev)
{
    cout << __func__ << endl;
    if (bev->get_obuf_length() == 0)
    {
        bev->remove_write_event();
        test_ok++;
        cout << "write end\n";
    }
}

static void http_errcb(buffer_event *bev)
{
    cout << __func__ << endl;
    test_ok = -2;
}

static void http_basic_test(void)
{
    cout << __func__ << endl;
    int fd = http_connect(host, port);

    auto base = std::make_shared<epoll_base>();
    auto bev = std::make_shared<buffer_event>(base, fd);
    bev->register_readcb(http_readcb, bev.get());
    bev->register_writecb(http_writecb, bev.get());
    bev->register_errorcb(http_errcb, bev.get());

    bev->write_string("GET /test HTTP/1.1\r\nHost: some");
    bev->write_string("host\r\nConnection: close\r\n\r\n");

    bev->add_read_event();
    bev->add_write_event();

    base->loop();
}

static void http_request_done(std::shared_ptr<http_request> req)
{
    cout << __func__ << endl;
    string what = "This is funny";

    if (req->response_code != HTTP_OK)
    {
        cerr << "1 FAILED\n";
        cerr << req->response_code << endl;
        exit(1);
    }

    if (req->input_headers["Content-Type"].empty())
    {
        cerr << "2 FAILED\n";
        exit(1);
    }

    if (req->input_buffer->get_length() != static_cast<int>(what.length()))
    {
        cerr << "3 FAILED\n";
        exit(1);
    }

    if (memcmp(req->input_buffer->get_data(), what.c_str(), what.length()) != 0)
    {
        cerr << "4 FAILED\n";
        exit(1);
    }
}

static void http_connection_test(int persistent)
{
    cout << __func__ << endl;
    auto client = make_shared<http_client>();
    client->set_timeout(15);

    auto conn = client->make_connection(host, port);
    auto req = make_shared<http_request>(conn);

    req->output_headers["Host"] = "somehost";
    req->uri = "/test";
    req->type = REQ_GET;

    req->set_cb(http_request_done);

    /* 
	 * if our connections are not supposed to be persistent; request
	 * a close from the server.
	 */
    if (!persistent)
        req->output_headers["Connection"] = "close";

    conn->make_request(req);

    client->run();
}

static void close_detect_end()
{
    cout << __func__ << endl;
    // base->set_terminated();
}

static void close_detect_done(std::shared_ptr<http_request> req)
{
    cout << __func__ << endl;
    // // cout<<"code="<<req->response_code<<endl;
    if (req == nullptr || req->response_code != HTTP_OK)
    {
        cerr << "FAILED\n";
        exit(1);
    }

    auto base = req->get_base();
    std::shared_ptr<time_event> tev = create_event<time_event>(base);
    tev->set_timer(3, 0);
    base->register_callback(tev, close_detect_end);
    base->add_event(tev);
}

std::weak_ptr<http_client_connection> delayed_conn;

static void close_detect_launch()
{
    cout << __func__ << endl;
    auto conn = delayed_conn.lock();
    if (!conn)
    {
        cerr << "FAILED with conn expired\n";
        exit(1);
    }

    auto req = make_shared<http_request>(conn);
    req->output_headers["Host"] = "somehost";
    req->type = REQ_GET;
    req->uri = "/test";
    req->set_cb(close_detect_done);

    // when detect if the connection is closed, the client send another request
    conn->make_request(req);
}

static void close_detect_cb(std::shared_ptr<http_request> req)
{
    cout << __func__ << endl;
    if (req != nullptr && req->response_code != HTTP_OK)
    {
        cerr << "FAILEd\n";
        exit(1);
    }

    auto base = req->get_base();
    auto ev = create_event<time_event>(base);
    ev->set_timer(5, 0);
    base->register_callback(ev, close_detect_launch);
    base->add_event(ev);
}

static void http_close_detection(int with_delay)
{
    cout << __func__ << endl;

    auto client = make_shared<http_client>();
    auto conn = client->make_connection(host, port);
    delayed_conn = conn;

    client->set_timeout(2);

    auto req = make_shared<http_request>(conn);
    req->output_headers["Host"] = "somehost";
    req->type = REQ_GET;
    req->set_cb(close_detect_cb);

    if (with_delay)
        req->uri = "/largedelay";
    else
        req->uri = "/test";

    conn->make_request(req);
    client->run();
}

static void http_postrequest_done(shared_ptr<http_request> req)
{
    cout << __func__ << " called\n";

    const string what = "This is funny";

    if (req->response_code != HTTP_OK)
    {
        cerr << "FAILED (response code)\n";
        exit(1);
    }

    if (req->input_headers["Content-Type"].empty())
    {
        cerr << "Failed (content type)\n";
        exit(1);
    }

    if (req->input_buffer->get_length() != static_cast<int>(what.size()))
    {
        cerr << "Faild length\n";
        exit(1);
    }
}

static void http_post_test(void)
{
    cout << __func__ << endl;

    auto client = make_shared<http_client>();
    auto conn = client->make_connection(host, port);
    auto req = make_shared<http_request>(conn);
    req->output_headers["Host"] = "somehost";
    req->output_buffer->push_back_string("Okay. not really printf");
    req->uri = "/postit";
    req->type = REQ_POST;
    req->set_cb(http_postrequest_done);

    conn->make_request(req);
    client->run();
}

static void http_failure_readcb(buffer_event *bev)
{
    cout << __func__ << " called\n";
    const string what = "400 Bad Request";
    if (bev->get_ibuf()->find_string(what) != nullptr)
    {
        bev->remove_read_event();
        bev->get_base()->set_terminated();
    }
}

static void http_failure_test(void)
{
    cout << __func__ << endl;

    int fd = http_connect(host, port);

    auto base = make_shared<epoll_base>();
    auto bev = make_shared<buffer_event>(base, fd);
    bev->register_readcb(http_failure_readcb, bev.get());
    bev->register_writecb(http_writecb, bev.get());
    bev->register_errorcb(http_errcb, bev.get());

    bev->write_string("illegal request\r\n");
    bev->add_read_event();
    bev->add_write_event();

    base->loop();
}

static void http_dispatcher_test_done(shared_ptr<http_request> req)
{
    cout << __func__ << " called\n";

    const string what = "dispatcher-test";

    if (req->response_code != HTTP_OK)
    {
        cerr << "FAILED\n";
        exit(1);
    }

    if (req->input_headers["Content-Type"].empty())
    {
        cerr << "Failed (content-tyep) " << endl;
        exit(1);
    }

    if (req->input_buffer->get_length() != static_cast<int>(what.length()))
    {
        cerr << "Failed length\n";
        exit(1);
    }

    // base->set_terminated();
}

static void http_dispatcher_test(void)
{
    cout << __func__ << endl;

    auto client = make_shared<http_client>();
    auto conn = client->make_connection(host, port);
    auto req = make_shared<http_request>(conn);
    req->set_cb(http_dispatcher_test_done);
    req->output_headers["Host"] = "somehost";
    req->type = REQ_GET;
    req->uri = "/?arg=val";

    conn->make_request(req);

    client->run();
}

static void http_multi_line_header_test(void)
{
    cout << __func__ << endl;

    int fd = http_connect(host, port);

    auto base = make_shared<epoll_base>();
    auto bev = make_shared<buffer_event>(base, fd);
    bev->register_readcb(http_readcb, bev.get());
    bev->register_writecb(http_writecb, bev.get());
    bev->register_errorcb(http_errcb, bev.get());

    const string http_start_request =
        "GET /test HTTP/1.1\r\n"
        "Host: somehost\r\n"
        "Connection: close\r\n"
        "X-Multi:  aaaaaaaa\r\n"
        " a\r\n"
        "\tEND\r\n"
        "X-Last: last\r\n"
        "\r\n";

    bev->write_string(http_start_request);

    bev->add_read_event();
    bev->add_write_event();
    base->loop();
}

static void http_request_bad(std::shared_ptr<http_request> req)
{
    cout << __func__ << endl;
    req->get_base()->set_terminated();
}

static void http_negative_content_length_test(void)
{
    cout << __func__ << endl;
    auto client = make_shared<http_client>();
    auto conn = client->make_connection(host, port);
    auto req = make_shared<http_request>(conn);
    req->set_cb(http_request_bad);
    req->output_headers["X-Negative"] = "makeitso";
    req->output_headers["Host"] = "somehost";
    req->type = REQ_GET;
    req->uri = "/test";

    conn->make_request(req);
    client->run();
}

static void http_chunked_readcb(buffer_event *bev)
{
    cout << __func__ << " called\n";
    if (bev->get_ibuf()->find_string("bwv 1052") != nullptr)
    {
        bev->get_base()->set_terminated();
    }
}

static void http_chunked_writecb(buffer_event *bev)
{
    cout << __func__ << " called\n";
    if (bev->get_obuf_length() == 0)
    {
        bev->remove_write_event();
        bev->add_read_event();
    }
}

static void http_chunked_errorcb(buffer_event *bev)
{
    cout << __func__ << " called\n";
}

static void http_chunked_test(void)
{
    cout << __func__ << endl;

    int fd = http_connect(host, port);

    auto base = make_shared<epoll_base>();
    auto bev = make_shared<buffer_event>(base, fd);
    bev->register_readcb(http_chunked_readcb, bev.get());
    bev->register_writecb(http_chunked_writecb, bev.get());
    bev->register_errorcb(http_chunked_errorcb, bev.get());

    const string http_request =
        "GET /chunked HTTP/1.1\r\n"
        "Host: somehost\r\n"
        "Connection: close\r\n"
        "\r\n";

    bev->write_string(http_request);
    bev->add_read_event();
    bev->add_write_event();
    base->loop();
}

static void request_chunked_done(shared_ptr<http_request> req)
{
    cout << __func__ << " called\n";
    if (req->input_buffer->find_string(CHUNKS[2]) != nullptr)
    {
        // base->set_terminated();
    }
}

static void http_chunked_handle_test(void)
{
    cout << __func__ << endl;
    auto client = make_shared<http_client>();
    auto conn = client->make_connection(host, port);
    auto req = make_shared<http_request>(conn);

    req->set_cb(request_chunked_done);
    req->output_headers["Connection"] = "close";
    req->output_headers["Host"] = "somehost";
    req->type = REQ_GET;
    req->uri = "/chunked";

    conn->make_request(req);
    client->run();
}

static int count_req = 1;
static void keep_alive_cb(shared_ptr<http_request> req)
{
    cout << "count=" << count_req++ << endl;
    cout<<req->input_buffer->get_data()<<endl;
    cout<<req->uri<<endl;
}

static void http_keepalive_test(void)
{
    cout << __func__ << endl;
    auto client = make_shared<http_client>();
    auto conn = client->make_connection(host, port);

    vector<shared_ptr<http_request>> reqs;

    for (int i = 0; i < 20; i++)
    {
        auto req = make_shared<http_request>(conn);

        req->set_cb(keep_alive_cb);
        req->output_headers["Connection"] = "keep-alive";
        req->output_headers["Host"] = "somehost";
        req->type = REQ_GET;
        req->uri = "/keep/"+to_string(i);

        conn->make_request(req);
        reqs.push_back(req);
    }

    client->run();
}

int main(int argc, char const *argv[])
{
    init_log_file("regress_http_client.log");
    http_basic_test();

    http_connection_test(0);
    http_connection_test(1);

    http_close_detection(0);
    http_close_detection(1);

    http_post_test();

    http_failure_test();

    http_dispatcher_test();

    http_multi_line_header_test();

    http_negative_content_length_test();

    http_chunked_test();
    http_chunked_handle_test();

    http_keepalive_test();
    std::cout << "succeed\n";
    return 0;
}
