#pragma once

#include <functional>

#include <rw_event.hh>
#include <buffer.hh>

namespace eve
{

class buffer_event;
using BufferEvCallback = std::function<void(buffer_event *)>;

class buffer_event : public rw_event
{
public:
  std::shared_ptr<buffer> input_buffer;
  std::shared_ptr<buffer> output_buffer;
  BufferEvCallback readcb;
  BufferEvCallback writecb;
  BufferEvCallback errorcb;

public:
public:
  buffer_event(std::shared_ptr<event_base> base) : rw_event(base) { init(); }
  buffer_event(std::shared_ptr<event_base> base, int fd, TYPE t) : rw_event(base, fd, t) { init(); }

  ~buffer_event() {}

  void init();

  inline void set_readcb(BufferEvCallback cb) { readcb = cb; }
  inline void set_writecb(BufferEvCallback cb) { writecb = cb; }
  inline void set_errorcb(BufferEvCallback cb) { errorcb = cb; }

  inline void set_cb(BufferEvCallback readcb, BufferEvCallback writecb, BufferEvCallback errcb)
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
  static void buffer_event_cb(buffer_event *ev);

  static void default_cb(buffer_event *ev);
};

} // namespace eve
