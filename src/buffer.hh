#include <sys/types.h>

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
	size_t _totallen=0;

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
	int remove(void *data, size_t datlen);
	char *readline();

	/* operation with file descriptior */
	int readfd(int fd, int howmuch);
	int writefd(int fd);

	/* push_back and pop_front */
	int push_back(void *data, size_t datlen);
	int push_back_buffer(buffer *inbuf);
	size_t pop_front(void *data, size_t size);

	unsigned char *find(unsigned char *what, size_t len);

	inline int get_off() const { return _off; }

};

} // namespace eve
