#include <sys/types.h>

namespace eve
{

#define BUFFER_MAX_READ 4096

class buffer
{
  private:
    unsigned char *_origin_buf;
    unsigned char *_buf;
    size_t _misalign;
    size_t _off;
    size_t _totallen;

  private:
    void align();

  public:
    buffer(/* args */)
    {
        _origin_buf = new unsigned char[128];
    }
    ~buffer()
    {
        delete _origin_buf;
    }
    int add_buffer(buffer *inbuf);
    int remove(void *data, size_t datlen);
    char *readline();
    int expand(size_t datlen);
    int add(void *data, size_t datlen);
    void drain(size_t len);

    /* operation with file descriptior */
    int readfd(int fd, int howmuch);
    int writefd(int fd);

    unsigned char *find(unsigned char *what, size_t len);
};

} // namespace eve
