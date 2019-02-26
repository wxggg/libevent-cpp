#include <http_server.hh>
#include <epoll_base.hh>

#include <memory>

using namespace std;
using namespace eve;

void home(shared_ptr<http_request> req)
{
    
}

int main(int argc, char const *argv[])
{
    auto base = make_shared<epoll_base>();
    auto server = make_shared<http_server>(base);

    server->set_handle_cb("/", home);

    return 0;
}
