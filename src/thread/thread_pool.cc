#include <thread_pool.hh>

namespace eve
{

void thread_pool::resize(int nThreads)
{
    if (!isStop && !isDone)
    {
        int currentThreads = threads.size();
        if (currentThreads <= nThreads) // need more threads
        {
            threads.resize(nThreads);
            flags.resize(nThreads);

            for (int i = currentThreads; i < nThreads; i++)
            {
                flags[i] = std::make_shared<std::atomic<bool>>(false);
                set_thread(i);
            }
        }
        else // close additional threads
        {
            for (int i = nThreads; i < currentThreads; i++)
            {
                *flags[i] = true;
                threads[i]->detach();
            }
            // stop the detached threads that were waiting
            {
                Lock lock(mutex);
                cv.notify_all();
            }
            threads.resize(nThreads);
            flags.resize(nThreads);
        }
    }
}


void thread_pool::set_thread(int i)
{
    std::shared_ptr<std::atomic<bool>> flag(flags[i]);
    auto f = [this, i, flag]() {
        std::atomic<bool> &flagi = *flag;
        Task *t;
        bool isPop = this->taskQueue.pop(t);
        while (true)
        {
            while (isPop)
            {
                std::unique_ptr<Task> t_(t); // used to delete t at return
                (*t)();
                if (flagi)
                    return;
                else
                    isPop = this->taskQueue.pop(t);
            }
            // here the queue is empty, wait for the next task
            Lock lock(this->mutex);
            ++this->nWaiting;
            this->cv.wait(lock, [this, &t, &isPop, &flagi]() {
                isPop = this->taskQueue.pop(t);
                return isPop || this->isDone || flagi;
            });
            --this->nWaiting;
            if (!isPop) // if isDone or flagi
                return;
        }
    };
    threads[i].reset(new std::thread(f));
}

void thread_pool::stop(bool isWait)
{
    if (!isWait)
    {
        if (isStop)
            return;
        isStop = true;
        for (int i = 0, n = size(); i < n; i++)
            *flags[i] = true;
        clear_task_queue();
    }
    else
    {
        if (isDone || isStop)
            return;
        isDone = true;
    }

    {
        Lock lock(mutex); // 这个锁在出大括号时就会被打开
        cv.notify_all();
    }

    for (int i = 0; i < size(); i++)
        if (threads[i]->joinable())
            threads[i]->join();
    clear_task_queue();
    threads.clear();
    flags.clear();
}

} // namespace eve
