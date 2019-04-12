#include <http_server.hh>
#include <epoll_base.hh>

#include <memory>
#include <chrono>

using namespace std;
using namespace eve;

void home(http_request * req)
{
    auto buf = std::unique_ptr<buffer>(new buffer);
    buf->push_back_string("This is funnyThis is funnyThis iunny");

    /* allow sending of an empty reply */
    req->send_reply(HTTP_OK, "Everything is fine", req->input_headers["Empty"].empty() ? std::move(buf) : nullptr);
}

int main(int argc, char const *argv[])
{
    init_log_file("server.log");
    auto server = std::unique_ptr<http_server>(new http_server);
    server->set_timeout(15);

    server->set_handle_cb("/", home);
    server->resize_thread_pool(4);
    server->start("localhost", 8088);

    return 0;
}
