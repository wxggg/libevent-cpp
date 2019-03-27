#pragma once

#include <vector>
#include <memory>
#include <queue>

namespace eve
{

template <typename Alloc>
struct alloc_deleter
{
    alloc_deleter(Alloc *a) : a(a) {}

    void operator()(typename Alloc::pointer p) const
    {
        a->deallocate(p);
    }

  private:
    Alloc *a;
};

template <typename T, size_t BLOCK_SIZE = 4096>
class pool
{
    using Deleter = alloc_deleter<pool<T>>;

  private:
    std::vector<void *> blocks;
    std::queue<T *> freeQueue;
    T *begin;

    int sizePerBlock;
    int i; // current T pos in block, from 0 to itemsPerBlock-1

  public:
    typedef T value_type;
    typedef T *pointer;
    typedef T &reference;
    typedef const T *const_pointer;
    typedef const T &const_reference;
    typedef std::unique_ptr<T, Deleter> unique_ptr_type;
    pool()
    {
        sizePerBlock = static_cast<int>(BLOCK_SIZE / sizeof(T));
        i = sizePerBlock; // at first there's none T
        begin = nullptr;
    }
    ~pool() {}

    template <typename... Args>
    inline decltype(auto) allocate_unique(Args &&... args)
    {
        auto p = allocate();
        new (p) T(std::forward<Args>(args)...);
        return std::unique_ptr<T, Deleter>(p, Deleter(this));
    }

    void allocate_block()
    {
        void *newBlock = operator new(BLOCK_SIZE);
        i = 0;
        blocks.push_back(newBlock);
        begin = reinterpret_cast<T *>(newBlock);
    }

    inline decltype(auto) allocate()
    {
        if (freeQueue.empty())
        {
            if (i >= sizePerBlock || begin == nullptr)
                allocate_block();
            auto p = begin + i;
            i++;
            return p;
        }
        else
        {
            auto p = freeQueue.front();
            freeQueue.pop();
            return p;
        }
    }

    inline void deallocate(T *p)
    {
        freeQueue.push(p);
    }
};

} // namespace eve
