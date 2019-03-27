#include <pool.hh>
#include <http_server.hh>
#include <http_server_connection.hh>
#include <buffer.hh>

#include <iostream>
#include <memory>

using namespace std;
using namespace eve;

int main(int argc, char const *argv[])
{
    pool<http_server> sp;
    auto p = sp.allocate_unique();

    pool<http_server_connection> cp;

    auto c = cp.allocate_unique(nullptr, -1, nullptr);

    return 0;
}
