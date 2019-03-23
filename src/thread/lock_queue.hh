#pragma once

#include <queue>
#include <mutex>

namespace eve
{

using Lock = std::unique_lock<std::mutex>;

template <typename T>
class lock_queue
{
  private:
    std::mutex mutex;
    std::queue<T> q;

  public:
    bool push(T &&v)
    {
        Lock lock(mutex);
        q.push(std::move(v));
        return true;
    }

    bool pop(T &v)
    {
        Lock lock(mutex);
        if (q.empty())
            return false;
        v = std::move(q.front());
        q.pop();
        return true;
    }

    bool empty()
    {
        Lock lock(mutex);
        return q.empty();
    }

    int size()
    {
        Lock lock(mutex);
        return q.size();
    }
};

} // namespace eve
