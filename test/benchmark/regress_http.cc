#include <http_server.hh>
#include <epoll_base.hh>
#include <poll_base.hh>
#include <buffer_event.hh>
#include <util_network.hh>
#include <http_client.hh>
#include <time_event.hh>

#include <iostream>
#include <string>

using namespace std;
using namespace eve;

std::shared_ptr<event_base> base = nullptr;
static int test_ok = 0;
std::string host = "127.0.0.1";
unsigned short port = 8102;

void http_test_cb(std::shared_ptr<http_request> req)
{
    cerr << __func__ << " called\n";
    std::shared_ptr<buffer> buf = std::make_shared<buffer>();
    buf->push_back_string("This is funny");
    const std::string &multi = req->input_headers["X-multi"];
    if (!multi.empty())
    {
        if (multi.substr(multi.length() - 3) == "END")
            test_ok++;
        if (!req->input_headers["X-Last"].empty())
            test_ok++;
    }

    /* injecting a bad content-length */
    if (!req->input_headers["X-Negative"].empty())
        req->output_headers["Content-Length"] = "-100";

    /* allow sending of an empty reply */
    req->send_reply(HTTP_OK, "Everything is fine", req->input_headers["Empty"].empty() ? buf : nullptr);
}

static const string CHUNKS[] = {
    "This is funny",
    "but no hilarious.",
    "bwv 1052"};

struct chunk_req_state
{
    shared_ptr<http_request> req;
    int i;
};

static void
http_chunked_trickle_cb(time_event *ev)
{
    cerr << __func__ << " called!!\n";
    struct chunk_req_state *state = (struct chunk_req_state *)ev->data;

    std::shared_ptr<buffer> buf = std::make_shared<buffer>();
    buf->push_back_string(CHUNKS[state->i]);
    state->req->send_reply_chunk(buf);

    if (++state->i < static_cast<int>(sizeof(CHUNKS) / sizeof(CHUNKS[0])))
    {
        ev->set_timer(0, 0);
        ev->add();
    }
    else
    {
        state->req->send_reply_end();
        delete state;
    }
}

void http_chunked_cb(std::shared_ptr<http_request> req)
{
    cerr << __func__ << " called!!\n";

    struct chunk_req_state *state = new chunk_req_state;
    state->req = req;
    state->i = 0;

    req->send_reply_start(HTTP_OK, "Everything is fine");

    time_event *tev = new time_event(base);
    tev->set_timer(0, 0);
    tev->set_callback(http_chunked_trickle_cb, tev);
    tev->data = (void *)state;
    tev->add();
}

void http_post_cb(std::shared_ptr<http_request> req)
{
    cout << __func__ << " called\n";

    if (req->type != REQ_POST)
    {
        cerr << "FAILED (post type)\n";
        exit(1);
    }

    cout << "get data::" << req->input_buffer->get_data() << endl;

    std::shared_ptr<buffer> buf = std::make_shared<buffer>();
    buf->push_back_string("This is funny");

    req->send_reply(HTTP_OK, "Everything is find", buf);
}

http_client_connection *delayed_conn;
static void http_delay_reply(time_event *ev, shared_ptr<http_request> req)
{
    cerr << __func__ << " called" << endl;
    req->send_reply(HTTP_OK, "Everything is fine", nullptr);

    delayed_conn->fail(HTTP_EOF);
}

void http_large_delay_cb(std::shared_ptr<http_request> req)
{
    cerr << __func__ << " called!!!\n";
    time_event *tev = new time_event(base);
    tev->set_timer(6, 0);
    tev->set_callback(http_delay_reply, tev, req);
    tev->add();
}

void http_dispatcher_cb(std::shared_ptr<http_request> req)
{
    cerr << __func__ << " called!!!\n";
    std::shared_ptr<buffer> buf = std::make_shared<buffer>();
    buf->push_back_string("dispatcher-test");

    req->send_reply(HTTP_OK, "Everything is find", buf);
}

static shared_ptr<http_server> http_setup(std::shared_ptr<event_base> base)
{
    cout << __func__ << endl;
    shared_ptr<http_server> server(new http_server(base));

    server->set_handle_cb("/test", http_test_cb);
    server->set_handle_cb("/chunked", http_chunked_cb);
    server->set_handle_cb("/postit", http_post_cb);
    server->set_handle_cb("/largedelay", http_large_delay_cb);
    server->set_handle_cb("/", http_dispatcher_cb);

    server->start(host, port);
    return server;
}

static void http_readcb(buffer_event *bev)
{
    cout << __func__ << endl;
    if (bev->input_buffer->find_string("This is funny") != nullptr)
    {
        std::shared_ptr<http_request> req(new http_request());
        // enum message_read_status done;

        req->kind = RESPONSE;
        req->parse_firstline(bev->input_buffer);
        req->parse_headers(bev->input_buffer);
        for (const auto &p : req->input_headers)
            cout << p.first << "\t===\t" << p.second << endl;
        bev->del_read();
        base->set_terminated(true);
    }
    else
    {
        cout << "not found\n";
    }
}

static void http_writecb(buffer_event *bev)
{
    cout << __func__ << endl;
    if (bev->get_obuf_length() == 0)
    {
        bev->del_write();
        test_ok++;
        cout << "write end\n";
    }
}

static void http_errcb(buffer_event *bev)
{
    cout << __func__ << endl;
    base->set_terminated(true);
    test_ok = -2;
}

static void http_basic_test(void)
{
    cout << __func__ << endl;
    std::shared_ptr<http_server> server = http_setup(base);

    int fd = http_connect(host, port);

    buffer_event *bev = new buffer_event(base, fd, RDWR);
    bev->set_cb(http_readcb, http_writecb, http_errcb);

    bev->write_string("GET /test HTTP/1.1\r\nHost: some");
    bev->write_string("host\r\nConnection: close\r\n\r\n");
    bev->add();

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

    base->set_terminated(true);
}

static void http_connection_test(int persistent)
{
    cout << __func__ << endl;
    std::shared_ptr<http_server> server = http_setup(base);
    shared_ptr<http_client> client(new http_client(base));

    int connid = client->make_connection(host, port);

    std::shared_ptr<http_request> req(new http_request());
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

    client->make_request(connid, req);

    base->loop();
}

static void close_detect_end()
{
    cout << __func__ << endl;
    base->set_terminated(true);
}

static void close_detect_done(std::shared_ptr<http_request> req)
{
    cout << __func__ << endl;
    // cout<<"code="<<req->response_code<<endl;
    if (req == nullptr || req->response_code != HTTP_OK)
    {
        cerr << "FAILED\n";
        exit(1);
    }

    time_event *tev = new time_event(base);
    tev->set_timer(3, 0);
    tev->set_callback(close_detect_end);
    tev->add();
}

static void close_detect_launch()
{
    cout << __func__ << endl;

    std::shared_ptr<http_request> req(new http_request());
    req->output_headers["Host"] = "somehost";
    req->type = REQ_GET;
    req->uri = "/test";
    req->set_cb(close_detect_done);

    delayed_conn->client->make_request(delayed_conn->id, req);
}

static void close_detect_cb(std::shared_ptr<http_request> req)
{
    cout << __func__ << endl;
    if (req != nullptr && req->response_code != HTTP_OK)
    {
        cerr << "FAILEd\n";
        exit(1);
    }

    time_event *tev = new time_event(base);
    tev->set_timer(3, 0);
    tev->set_callback(close_detect_launch);

    tev->add();
}

static void http_close_detection(int with_delay)
{
    cout << __func__ << endl;
    std::shared_ptr<http_server> server = http_setup(base);
    server->set_timeout(5);

    shared_ptr<http_client> client(new http_client(base));
    int connid = client->make_connection(host, port);
    delayed_conn = client->get_connection(connid).get();

    std::shared_ptr<http_request> req(new http_request());
    req->output_headers["Host"] = "somehost";
    req->type = REQ_GET;
    req->set_cb(close_detect_cb);

    if (with_delay)
        req->uri = "/largedelay";
    else
        req->uri = "/test";

    client->make_request(connid, req);

    base->loop();
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

    base->set_terminated(true);
}

static void http_post_test(void)
{
    cout << __func__ << endl;
    shared_ptr<http_server> server = http_setup(base);
    cout << "Testing HTTP Post Request: ";

    shared_ptr<http_client> client(new http_client(base));
    shared_ptr<http_request> req(new http_request);

    req->output_headers["Host"] = "somehost";
    req->output_buffer->push_back_string("Okay. not really printf");
    req->uri = "/postit";
    req->type = REQ_POST;
    req->set_cb(http_postrequest_done);

    int connid = client->make_connection(host, port);
    client->make_request(connid, req);

    base->loop();
}

static void http_failure_readcb(buffer_event *bev)
{
    cout << __func__ << " called\n";
    const string what = "400 Bad Request";
    if (bev->input_buffer->find_string(what) != nullptr)
    {
        bev->del_read();
        base->set_terminated(true);
    }
}

static void http_failure_test(void)
{
    cout << __func__ << endl;
    std::shared_ptr<http_server> server = http_setup(base);

    int fd = http_connect(host, port);

    buffer_event *bev = new buffer_event(base, fd, RDWR);
    bev->set_cb(http_failure_readcb, http_writecb, http_errcb);

    bev->write_string("illegal request\r\n");
    bev->add();

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

    base->set_terminated(true);
}

static void http_dispatcher_test(void)
{
    cout << __func__ << endl;
    cout << "Testing HTTP Dispatcher: " << endl;

    std::shared_ptr<http_server> server = http_setup(base);
    shared_ptr<http_client> client(new http_client(base));
    int connid = client->make_connection(host, port);

    shared_ptr<http_request> req(new http_request);
    req->set_cb(http_dispatcher_test_done);
    req->output_headers["Host"] = "somehost";
    req->type = REQ_GET;
    req->uri = "/?arg=val";

    client->make_request(connid, req);

    base->loop();
}

static void http_multi_line_header_test(void)
{
    cout << __func__ << endl;
    std::shared_ptr<http_server> server = http_setup(base);

    int fd = http_connect(host, port);

    buffer_event *bev = new buffer_event(base);
    bev->set_fd(fd);
    bev->set_read();
    bev->set_write();
    bev->set_cb(http_readcb, http_writecb, http_errcb);

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
    bev->add();

    base->loop();
}

static void http_request_bad(std::shared_ptr<http_request> req)
{
    cout << __func__ << endl;
    base->set_terminated(true);
}

static void http_negative_content_length_test(void)
{
    cout << __func__ << endl;
    std::shared_ptr<http_server> server = http_setup(base);
    shared_ptr<http_client> client(new http_client(base));
    int connid = client->make_connection(host, port);

    shared_ptr<http_request> req(new http_request);
    req->set_cb(http_request_bad);
    req->output_headers["X-Negative"] = "makeitso";
    req->output_headers["Host"] = "somehost";
    req->type = REQ_GET;
    req->uri = "/test";

    client->make_request(connid, req);

    base->loop();
}

static void http_chunked_readcb(buffer_event *bev)
{
    cout << __func__ << " called\n";

    if (bev->input_buffer->find_string("bwv 1052") != nullptr)
    {
        base->set_terminated(true);
    }
}

static void http_chunked_writecb(buffer_event *bev)
{
    cout << __func__ << " called\n";
    if (bev->get_obuf_length() == 0)
    {
        bev->del_write();
        bev->add_read();
    }
}

static void http_chunked_errorcb(buffer_event *bev)
{
    cout << __func__ << " called\n";
}

static void http_chunked_test(void)
{
    cout << __func__ << endl;
    std::shared_ptr<http_server> server = http_setup(base);

    int fd = http_connect(host, port);

    buffer_event *bev = new buffer_event(base, fd, RDWR);
    bev->set_cb(http_chunked_readcb, http_chunked_writecb, http_chunked_errorcb);

    const string http_request =
        "GET /chunked HTTP/1.1\r\n"
        "Host: somehost\r\n"
        "Connection: close\r\n"
        "\r\n";

    bev->write_string(http_request);
    bev->add();

    base->loop();
}

static void request_chunked_done(shared_ptr<http_request> req)
{
    cout << __func__ << " called\n";
    if (req->input_buffer->find_string(CHUNKS[2]) != nullptr)
    {
        base->set_terminated(true);
    }
}

static void http_chunked_handle_test(void)
{
    cout << __func__ << endl;
    std::shared_ptr<http_server> server = http_setup(base);
    shared_ptr<http_client> client(new http_client(base));
    int connid = client->make_connection(host, port);

    shared_ptr<http_request> req(new http_request);
    req->set_cb(request_chunked_done);
    req->output_headers["Connection"] = "close";
    req->output_headers["Host"] = "somehost";
    req->type = REQ_GET;
    req->uri = "/chunked";

    client->make_request(connid, req);

    base->loop();
}

int main(int argc, char const *argv[])
{
    base = std::make_shared<epoll_base>();
    // base = new poll_base();

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

    std::cout << "succeed\n";
    return 0;
}
