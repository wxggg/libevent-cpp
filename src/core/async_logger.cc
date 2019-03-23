#include <async_logger.hh>
#include <buffer.hh>

#include <assert.h>
#include <string.h>
#include <iostream>

namespace eve
{

async_logger::async_logger()
{
    for (int i = 0; i < 2; i++)
    {
        auto buf = std::make_unique<buffer>();
        buf->resize(4 * 1024 * 1024);
        emptyQueue.push(std::move(buf));
    }

    emptyQueue.pop(input);

    out.open(logFile);

    running = true;
    logThread = std::make_unique<std::thread>([=] { gather(); });
}

async_logger::~async_logger()
{
    running = false;
    {
        Lock lock(mutex);
        cv.notify_all();
    }
    if (logThread->joinable())
        logThread->join();
}

void async_logger::set_log_file(const std::string &file)
{
    logFile = file;
    out.close();
    std::ofstream().swap(out);
    out.open(logFile);
}

std::unique_ptr<buffer> async_logger::get_empty_buffer()
{
    std::unique_ptr<buffer> buf;
    emptyQueue.pop(buf);
    if (!buf)
    {
        buf = std::unique_ptr<buffer>();
        buf->resize(4 * 1024 * 1024);
    }
    return buf;
}

void async_logger::append(const std::string &line)
{
    Lock lock(input_mutex);
    input->push_back_string(line);
    if (input->get_length() > 4 * 1000 * 1000)
    {
        outputQueue.push(std::move(input));
        input = get_empty_buffer();
        cv.notify_all();
    }
}

void async_logger::gather()
{
    assert(running);

    while (running || !outputQueue.empty())
    {
        if (outputQueue.empty())
        {
            Lock lock(mutex); // no other thread use this mutex
            cv.wait_for(lock, std::chrono::seconds(3));
            if (outputQueue.empty())
            {
                {
                    Lock lock(input_mutex);
                    if (input->get_length() > 0)
                    {
                        out.write(input->get_data(), input->get_length());
                        input->reset();
                    }
                }
            }
        }
        else
        {
            std::unique_ptr<buffer> output;
            while (outputQueue.pop(output))
            {
                out.write(output->get_data(), output->get_length());
                output->reset();
                emptyQueue.push(std::move(output));
            }
        }
        out.flush();
    }

    if (input->get_length() > 0)
    {
        out.write(input->get_data(), input->get_length());
        input->reset();
        out.flush();
    }
    out.close();
}

} // namespace eve
