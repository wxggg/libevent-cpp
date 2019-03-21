#include <async_logger.hh>
#include <buffer.hh>

#include <assert.h>
#include <string.h>
#include <iostream>

namespace eve
{

async_logger::async_logger()
{
    for (int i = 0; i < 4; i++)
    {
        auto buf = std::make_shared<buffer>();
        buf->resize(4096);
        inputQueue.push(buf);
    }

    inputQueue.pop(input);

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

std::shared_ptr<buffer> async_logger::get_empty_buffer()
{
    std::shared_ptr<buffer> buf;
    if (emptyQueue.pop(buf) == false)
    {
        buf = std::make_shared<buffer>();
        buf->resize(4096);
    }
    return buf;
}

void async_logger::append(const std::string &line)
{
    Lock lock(input_mutex);
    input->push_back_string(line);
    if (input->get_length() > 4000)
    {
        outputQueue.push(input);
        input = get_empty_buffer();
        cv.notify_all();
    }
}

void async_logger::gather()
{
    assert(running);

    static int i = 0;
    while (running || !outputQueue.empty())
    {
        if (outputQueue.empty())
        {
            Lock lock(no_use_mutex); // no other thread use this mutex
            cv.wait_for(lock, std::chrono::seconds(2));
            if (outputQueue.empty())
            {
                {
                    Lock lock(input_mutex);
                    if (input->get_length() > 0)
                    {
                        output = input;
                        input = get_empty_buffer();
                    }
                }
                out.write(output->get_data(), output->get_length());
                output->reset();
                emptyQueue.push(output);
            }
        }
        else
        {
            while (outputQueue.pop(output))
            {
                out.write(output->get_data(), output->get_length());
                output->reset();
                emptyQueue.push(output);
            }
        }
        out.flush();
    }

    if (input->get_length() > 0)
    {
        out.write(input->get_data(), input->get_length());
        out.flush();
    }
    out.close();
}

} // namespace eve
