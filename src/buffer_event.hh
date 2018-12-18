#include "rw_event.hh"
#include "buffer.hh"

namespace eve
{

class buffer_event;
typedef void (*bufferevent_cb_t)(buffer_event *);

class buffer_event : public rw_event
{
  private:
    buffer ibuf;
    buffer obuf;
    bufferevent_cb_t readcb = nullptr;
    bufferevent_cb_t writecb = nullptr;
    bufferevent_cb_t errorcb = nullptr;

  public:
  public:
    buffer_event(event_base *base);
    ~buffer_event() {}

    inline void set_readcb(bufferevent_cb_t cb) { readcb = cb; }
    inline void set_writecb(bufferevent_cb_t cb) { writecb = cb; }
    inline void set_errorcb(bufferevent_cb_t cb) { errorcb = cb; }

    inline void set_cb(bufferevent_cb_t readcb, bufferevent_cb_t writecb, bufferevent_cb_t errcb)
    {
        this->readcb = readcb;
        this->writecb = writecb;
        this->errorcb = errcb;
    }

    inline int get_ibuf_length() const { return ibuf.get_off(); }
    inline int get_obuf_length() const { return obuf.get_off(); }

    size_t write(void *data, size_t size);
    size_t read(void *data, size_t size);

  private:
    static void buffer_event_cb(event *ev);

    static void default_cb(buffer_event *ev);
};

} // namespace eve
