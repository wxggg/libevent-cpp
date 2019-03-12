#pragma once

#include <list>
#include <mutex>
#include <iostream>

namespace eve
{

using Lock = std::unique_lock<std::mutex>;

template <typename T>
class lock_list
{
  private:
    std::mutex mutex;
    std::list<T> l;

  public:
    void insert(const T &v)
    {
        Lock lock(mutex);
        l.insert(v);
    }

    void push_back(const T &v)
    {
        Lock lock(mutex);
        l.push_back(v);
    }

    void remove(const T &v)
    {
        Lock lock(mutex);
        l.remove(v);
    }

    template <class Predicate>
    void remove_if(Predicate pred)
    {
        Lock lock(mutex);
        l.remove_if(pred);
    }

    void clear()
    {
        Lock lock(mutex);
        l.clear();
    }

    bool empty()
    {
        Lock lock(mutex);
        return l.empty();
    }

    int size()
    {
        Lock lock(mutex);
        return l.size();
    }
};

} // namespace eve
