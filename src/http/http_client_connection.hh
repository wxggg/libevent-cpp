#pragma once

#include <http_connection.hh>

namespace eve
{

class http_client;
class http_client_connection : public http_connection
{
  public:
	std::string servaddr;
	unsigned int servport;

	std::weak_ptr<http_client> client;

	int retry_cnt = 0; /* retry count */
	int retry_max = 0; /* maximum number of retries */

  public:
	http_client_connection(std::shared_ptr<event_base> base, int fd, std::shared_ptr<http_client> client);
	~http_client_connection() {}

	void fail(http_connection_error error);

	void do_read_done();
	void do_write_done();

	int connect();
	int make_request(std::unique_ptr<http_request> req);
};

} // namespace eve
