#include "buffer.hh"
#include <string>
#include <iostream>
#include <unistd.h>
#include <string.h>

namespace eve
{

int buffer::add_buffer(buffer *inbuf)
{
    int res = add(inbuf->_buf, inbuf->_off);
    if (res == 0)
        inbuf->drain(inbuf->_off);

    return res;
}

/* read nread data from the buf, then train the bytes read */
int buffer::remove(void *data, size_t datlen)
{
    size_t nread = datlen;
    if (nread >= _off)
        nread = _off;

    memcpy(data, _buf, nread);
    drain(nread);

    return nread;
}

/*
 * Reads a line terminated by either '\r\n', '\n\r' or '\r' or '\n'.
 * The returned buffer needs to be freed by the called.
 */
char *buffer::readline()
{
    unsigned char *data = _buf;
    char *line;
    unsigned int i;

    for (i = 0; i < _off; i++)
    {
        if (data[i] == '\r' || data[i] == '\n')
            break;
    }

    if (i == _off)
        return nullptr; /* not found */

    try
    {
        line = new char[i + 1];
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        drain(i);
        return nullptr;
    }

    memcpy(line, data, i);
    line[i] = '\0';
    drain(i + 1);
    return line;
}

void buffer::align()
{
    memmove(_origin_buf, _buf, _off);
    _buf = _origin_buf;
    _misalign = 0;
}

int buffer::expand(size_t datlen)
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
    {
        align();
    }
    else
    {
        unsigned char *newbuf;
        size_t length = _totallen;

        if (length < 256)
            length = 256;
        while (length < need)
            length <<= 1;

        if (_origin_buf != _buf)
            align();
        if ((newbuf = (unsigned char *)realloc(_buf, length)) == NULL)
            return -1;

        _origin_buf = _buf = newbuf;
        _totallen = length;
    }
}

int buffer::add(void *data, size_t datlen)
{
    size_t need = _off + _misalign + datlen;
    size_t oldoff = _off;

    if (_totallen < need)
    {
        if (expand(datlen) == -1)
            return -1;
    }

    memcpy(_buf + _off, data, datlen);
    _off += datlen;

    return 0;
}

/**
 * remove the buf area [_buf, _buf+len]
 */
void buffer::drain(size_t len)
{
    if (len >= _off)
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

/* Reads data from a file descriptor into a buffer. */
int buffer::readfd(int fd, int howmuch)
{
    size_t oldoff = _off;
    int n = BUFFER_MAX_READ;

    if (howmuch < 0 || howmuch > n)
        howmuch = n;

    if (expand(howmuch) == -1)
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
    drain(n);
    return n;
}

unsigned char *buffer::find(unsigned char *what, size_t len)
{
    size_t remain = _off;
    auto search = _buf;
    unsigned char *p;

    while ((p = (unsigned char *)memchr(search, *what, remain)) != NULL && remain > len)
    {
        if (memcmp(p, what, len) == 0)
            return (unsigned char *)p;

        search = p + 1;
        remain = _off - (size_t)(search - _buf);
    }

    return NULL;
}

} // namespace eve
