#include <thread>
#include <condition_variable>
#include <mutex>
#include <string>
#include <fstream>
#include <iostream>

#include <lock_queue.hh>

namespace eve
{
using Lock = std::unique_lock<std::mutex>;

class buffer;
class time_event;
class rw_event;
class async_logger
{
  private:
    std::shared_ptr<buffer> input;
    std::shared_ptr<buffer> output;

    lock_queue<std::shared_ptr<buffer>> emptyQueue;

    lock_queue<std::shared_ptr<buffer>> inputQueue;
    lock_queue<std::shared_ptr<buffer>> outputQueue;

    std::unique_ptr<std::thread> logThread;

    bool running;
    std::mutex mutex;
    std::mutex input_mutex;
    std::mutex output_mutex;
    std::mutex no_use_mutex;
    std::condition_variable cv;

    std::string logFile = "default.log";

    std::ofstream out;

  public:
    async_logger();
    ~async_logger();

    void set_log_file(const std::string &file);

    void append(const std::string &line);

  private:
    std::shared_ptr<buffer> get_empty_buffer();
    void gather();
};

} // namespace eve
