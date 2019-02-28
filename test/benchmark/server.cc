#include <http_server.hh>
#include <epoll_base.hh>

#include <memory>

using namespace std;
using namespace eve;

void home(shared_ptr<http_request> req)
{
    cerr << __func__ << " called\n";
    std::shared_ptr<buffer> buf = std::make_shared<buffer>();
    buf->push_back_string("This is funny");

    /* allow sending of an empty reply */
    req->send_reply(HTTP_OK, "Everything is fine", req->input_headers["Empty"].empty() ? buf : nullptr);
}

int main(int argc, char const *argv[])
{
    auto base = make_shared<epoll_base>();
    auto server = make_shared<http_server>(base);

    server->set_handle_cb("/", home);
    server->start("localhost", 8080);

    base->loop();

    return 0;
}
