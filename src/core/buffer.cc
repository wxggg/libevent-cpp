#include <buffer.hh>
#include <string>
#include <cstring>
#include <unistd.h>

#include <logger.hh>

namespace eve
{

void buffer::reset()
{
    __drain(_off);
}

void buffer::resize(int n)
{
    __drain(_off);
    __expand(n);
}

/*
 * Reads a line terminated by either '\r\n', '\n\r' or '\r' or '\n'.
 * The returned buffer needs to be freed by the called.
 */
std::string buffer::readline()
{
    char *data = (char *)_buf;
    unsigned int i;

    for (i = 0; i < _off; i++)
    {
        if (data[i] == '\r' || data[i] == '\n')
            break;
    }

    if (i == _off)
        return ""; /* not found */

    int onemore = 0;
    if (i + 1 < _off) // check \r\n \n\r
    {
        if ((data[i] == '\r' && data[i + 1] == '\n') ||
            (data[i] == '\n' && data[i + 1] == '\r'))
            onemore++;
    }

    data[i] = '\0';
    data[i + onemore] = '\0';
    std::string line(data);
    __drain(i + onemore + 1);
    return line;
}

/* add data to the end of buffer */
int buffer::push_back(void *data, size_t datlen)
{
    size_t need = _off + _misalign + datlen;
    // size_t oldoff = _off;

    if (_totallen < need)
        if (__expand(datlen) == -1)
            return -1;

    std::memcpy(_buf + _off, data, datlen);
    _off += datlen;

    return 0;
}

int buffer::push_back_buffer(std::unique_ptr<buffer> &inbuf, int datlen)
{
    if (!inbuf)
        return 0;
    int len = datlen;
    if (len > static_cast<int>(inbuf->_off) || datlen < 0)
        len = inbuf->_off;
    int res = push_back(inbuf->_buf, len);
    if (res == 0)
        inbuf->__drain(len);

    return res;
}

/* read the font content in buffer to data */
size_t buffer::pop_front(void *data, size_t size)
{
    if (_off < size)
        size = _off;
    std::memcpy(data, _buf, size);

    if (size)
        __drain(size);
    return size;
}

/* Reads data from a file descriptor into a buffer. */
int buffer::readfd(int fd, int howmuch)
{
    int n = BUFFER_MAX_READ;

    if (howmuch < 0 || howmuch > n)
        howmuch = n;

    if (__expand(howmuch) == -1)
        return -1;

    unsigned char *p = _buf + _off;
    n = read(fd, p, howmuch);
    if (n == -1 || n == 0)
        return n;

    _off += n;
    return n;
}

int buffer::writefd(int fd)
{
    int n = write(fd, _buf, _off);
    if (n == -1 || n == 0)
        return n;
    __drain(n);
    return n;
}

unsigned char *buffer::find(unsigned char *what, size_t len)
{
    size_t remain = _off;
    auto search = _buf;
    unsigned char *p;

    while ((p = (unsigned char *)memchr(search, *what, remain)) != nullptr && remain > len)
    {
        if (std::memcmp(p, what, len) == 0)
            return (unsigned char *)p;

        search = p + 1;
        remain = _off - (size_t)(search - _buf);
    }

    return nullptr;
}

/** private function **/

/**
 * remove the buf area [_buf, _buf+len]
 */
void buffer::__drain(size_t len)
{
    if (len >= _off) // drain area bigger then buf to buf+off
    {
        _off = 0;
        _buf = _origin_buf;
        _misalign = 0;
    }
    else
    {
        _buf += len;
        _misalign += len;
        _off -= len;
    }
}

int buffer::__expand(size_t datlen)
{
    size_t need = _misalign + _off + datlen;

    /* capacity already satisfied */
    if (_totallen >= need)
        return 0;

    /**
         * if the buffer already have enough space
         * then only need to align the memory
         */
    if (_misalign >= datlen)
        __align();
    else
    {
        unsigned char *newbuf;
        size_t length = _totallen;

        if (length < 256)
            length = 256;
        while (length < need)
            length <<= 1;

        if (_origin_buf != _buf)
            __align();
        if ((newbuf = (unsigned char *)realloc(_buf, length)) == nullptr)
        {
            LOG_ERROR << "realloc error";
            return -1;
        }

        _origin_buf = _buf = newbuf;
        _totallen = length;
    }
    return 0;
}

void buffer::__align()
{
    std::memmove(_origin_buf, _buf, _off);
    _buf = _origin_buf;
    _misalign = 0;
}

} // namespace eve
