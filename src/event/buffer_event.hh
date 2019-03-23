#pragma once

#include <functional>

#include <rw_event.hh>
#include <buffer.hh>

namespace eve
{

class buffer_event
{
protected:
  std::unique_ptr<buffer> input;
  std::unique_ptr<buffer> output;
  std::shared_ptr<rw_event> ev = nullptr;

  std::weak_ptr<event_base> base;

public:
  std::shared_ptr<Callback> readcb = nullptr;
  std::shared_ptr<Callback> eofcb = nullptr;
  std::shared_ptr<Callback> writecb = nullptr;
  std::shared_ptr<Callback> errorcb = nullptr;

public:
  buffer_event(std::shared_ptr<event_base> base, int fd);
  ~buffer_event();

  template <typename F, typename... Rest>
  decltype(auto) register_readcb(F &&f, Rest &&... rest)
  {
    auto tsk = std::bind(std::forward<F>(f), std::forward<Rest>(rest)...);
    readcb = std::make_shared<Callback>([tsk]() { tsk(); });
  }

  template <typename F, typename... Rest>
  decltype(auto) register_eofcb(F &&f, Rest &&... rest)
  {
    auto tsk = std::bind(std::forward<F>(f), std::forward<Rest>(rest)...);
    eofcb = std::make_shared<Callback>([tsk]() { tsk(); });
  }

  template <typename F, typename... Rest>
  decltype(auto) register_writecb(F &&f, Rest &&... rest)
  {
    auto tsk = std::bind(std::forward<F>(f), std::forward<Rest>(rest)...);
    writecb = std::make_shared<Callback>([tsk]() { tsk(); });
  }

  template <typename F, typename... Rest>
  decltype(auto) register_errorcb(F &&f, Rest &&... rest)
  {
    auto tsk = std::bind(std::forward<F>(f), std::forward<Rest>(rest)...);
    errorcb = std::make_shared<Callback>([tsk]() { tsk(); });
  }

  inline void set_fd(int fd) { ev->set_fd(fd); }

  inline int get_ibuf_length() const { return input->get_length(); }
  inline int get_obuf_length() const { return output->get_length(); }
  inline const char *get_ibuf_data() const { return input->get_data(); }
  inline const char *get_obuf_data() const { return output->get_data(); }

  inline auto &get_ibuf() { return input; }
  inline auto &get_obuf() { return output; }

  decltype(auto) get_base()
  {
    auto b = base.lock();
    if (!b)
      std::cerr << "error base is expired\n";
    return b;
  }

  inline int fd() const { return ev->fd; }

  size_t write(void *data, size_t size);
  size_t read(void *data, size_t size);

  void add_read_event();
  void add_write_event();
  void remove_read_event();
  void remove_write_event();

  inline int write_out() { return output->writefd(ev->fd); }
  inline int read_in() { return input->readfd(ev->fd, -1); }

  inline size_t write_string(const std::string &s)
  {
    return output->push_back_string(s);
  }

  inline void write_buffer(std::unique_ptr<buffer> &buf)
  {
    output->push_back_buffer(buf, buf->get_length());
  }

private:
  static void rw_callback(buffer_event *bev);
};

} // namespace eve
