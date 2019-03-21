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
        // if (multi.substr(multi.length() - 3) == "END")
        //     test_ok++;
        // if (!req->input_headers["X-Last"].empty())
        //     test_ok++;
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
http_chunked_trickle_cb(std::shared_ptr<time_event> ev, struct chunk_req_state *state)
{
    cerr << __func__ << " called!!\n";
    std::shared_ptr<buffer> buf = std::make_shared<buffer>();
    buf->push_back_string(CHUNKS[state->i]);
    state->req->send_reply_chunk(buf);

    if (++state->i < static_cast<int>(sizeof(CHUNKS) / sizeof(CHUNKS[0])))
    {
        ev->set_timer(0, 0);
        ev->get_base()->add_event(ev);
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

    auto base = req->get_base();
    std::shared_ptr<time_event> tev = create_event<time_event>(base);
    tev->set_timer(0, 0);
    base->register_callback(tev, http_chunked_trickle_cb, tev, state);
    base->add_event(tev);
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

static void http_delay_reply(std::shared_ptr<time_event> ev, shared_ptr<http_request> req)
{
    cerr << __func__ << " called" << endl;
    req->send_reply(HTTP_OK, "Everything is fine", nullptr);
}

void http_large_delay_cb(std::shared_ptr<http_request> req)
{
    cerr << __func__ << " called!!!\n";
    auto base = req->get_base();
    auto ev = create_event<time_event>(base);
    ev->set_timer(3, 0);
    base->register_callback(ev, http_delay_reply, ev, req);
    base->add_event(ev);
}

void http_dispatcher_cb(std::shared_ptr<http_request> req)
{
    cerr << __func__ << " called!!!\n";
    std::shared_ptr<buffer> buf = std::make_shared<buffer>();
    buf->push_back_string("dispatcher-test");

    req->send_reply(HTTP_OK, "Everything is find", buf);
}

void http_keep_alive_cb(std::shared_ptr<http_request> req)
{
    cerr << __func__ << " called!!!\n";
    std::shared_ptr<buffer> buf = std::make_shared<buffer>();
    
    cout<<req->uri<<endl;

    req->send_reply(HTTP_OK, "Everything is find", buf);
}

static shared_ptr<http_server> http_setup()
{
    cout << __func__ << endl;
    auto server = make_shared<http_server>();
    server->resize_thread_pool(4);
    server->set_timeout(10);

    server->set_handle_cb("/test", http_test_cb);
    server->set_handle_cb("/chunked", http_chunked_cb);
    server->set_handle_cb("/postit", http_post_cb);
    server->set_handle_cb("/largedelay", http_large_delay_cb);
    server->set_handle_cb("/", http_dispatcher_cb);
    server->set_handle_cb("/keep/*", http_keep_alive_cb);

    server->start(host, port);
    return server;
}

int main(int argc, char const *argv[])
{
    init_log_file("regress_http_server.log");
    auto server = http_setup();
    server->start(host, port);
    return 0;
}
