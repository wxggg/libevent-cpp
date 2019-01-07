#include <string>

namespace eve
{

int set_fd_nonblock(int fd);
int get_nonblock_socket();

int bind_socket(const std::string &address, unsigned short port, int reuse);
int accept_socket(int fd, std::string &host, int &port);
int socket_connect(int fd, const std::string &address, unsigned short port);
int http_connect(const std::string &address, unsigned short port);

std::pair<int, int> get_fdpair();

int listenfd(int fd);
int check_socket(int socket);


} // namespace eve
