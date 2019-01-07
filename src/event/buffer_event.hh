#pragma once

#include <rw_event.hh>
#include <buffer.hh>

namespace eve
{

class buffer_event;
typedef void (*bufferevent_cb_t)(buffer_event *);

class buffer_event : public rw_event
{
public:
  buffer *input_buffer;
  buffer *output_buffer;
  bufferevent_cb_t readcb = nullptr;
  bufferevent_cb_t writecb = nullptr;
  bufferevent_cb_t errorcb = nullptr;

public:
public:
  buffer_event(event_base *base);
  ~buffer_event();

  inline void set_readcb(bufferevent_cb_t cb) { readcb = cb; }
  inline void set_writecb(bufferevent_cb_t cb) { writecb = cb; }
  inline void set_errorcb(bufferevent_cb_t cb) { errorcb = cb; }

  inline void set_cb(bufferevent_cb_t readcb, bufferevent_cb_t writecb, bufferevent_cb_t errcb)
  {
    if (readcb)
      this->readcb = readcb;
    if (writecb)
      this->writecb = writecb;
    if (errcb)
      this->errorcb = errcb;
  }

  inline int get_ibuf_length() const { return input_buffer->get_length(); }
  inline int get_obuf_length() const { return output_buffer->get_length(); }
  inline const char *get_ibuf_data() const { return input_buffer->get_data(); }
  inline const char *get_obuf_data() const { return output_buffer->get_data(); }

  size_t write(void *data, size_t size);
  size_t read(void *data, size_t size);

  inline int write_out() { return output_buffer->writefd(fd); }
  inline int read_in() { return input_buffer->readfd(fd, -1); }

  inline size_t write_string(const std::string &s)
  {
    return output_buffer->push_back_string(s);
  }

private:
  static void buffer_event_cb(event *ev);

  static void default_cb(buffer_event *ev);
};

} // namespace eve
