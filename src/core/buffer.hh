#pragma once

#include <string>
#include <memory>

namespace eve
{

#define BUFFER_MAX_READ 4096

#define DEFAULT_BUF_SIZE 128

class buffer
{
  private:
	unsigned char *_origin_buf;
	unsigned char *_buf;
	size_t _misalign = 0;
	size_t _off = 0;
	size_t _totallen = 0;

  private:
	void __align();
	int __expand(size_t datlen);
	void __drain(size_t len);

  public:
	buffer()
	{
		_totallen = DEFAULT_BUF_SIZE;
		_origin_buf = new unsigned char[_totallen];
		_buf = _origin_buf;
	}
	~buffer()
	{
		delete _origin_buf;
	}
	void reset();
	void resize(int n);
	int remove(void *data, size_t datlen);
	std::string readline();

	/* operation with file descriptior */
	int readfd(int fd, int howmuch);
	int writefd(int fd);

	/* push_back and pop_front */
	int push_back(void *data, size_t datlen);
	int push_back_buffer(std::shared_ptr<buffer> inbuf, int datlen);
	inline int push_back_string(const std::string &s)
	{
		return push_back((void *)s.c_str(), s.size());
	}
	size_t pop_front(void *data, size_t size);

	unsigned char *find(unsigned char *what, size_t len);
	inline unsigned char *find_string(const std::string & what)
	{
		return find((unsigned char *)what.c_str(), what.length());
	}

	inline int get_off() const { return _off; }
	inline int get_length() const { return _off; }
	inline const char *get_data() const { return (const char *)_buf; }
};

} // namespace eve
