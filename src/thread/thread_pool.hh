/**
 * thread pool implementioned with c++14
 * referenced by https://github.com/vit-vit/CTPL
 */

#include <lock_queue.hh>

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>

namespace eve
{

using Task = std::function<void()>;

class thread_pool
{
  private:
	std::vector<std::unique_ptr<std::thread>> threads;
	std::vector<std::shared_ptr<std::atomic<bool>>> flags; // true: stop the thread i
	lock_queue<Task *> taskQueue;

	std::atomic<int> nWaiting;
	std::atomic<bool> isDone;
	std::atomic<bool> isStop;

	std::mutex mutex;
	std::condition_variable cv;

  public:
	thread_pool() { init(); }
	thread_pool(int nThreads)
	{
		init();
		resize(nThreads);
	}

	~thread_pool() { stop(true); }

	inline void init() { nWaiting = 0, isStop = false, isDone = false; }

	inline int size() { return static_cast<int>(threads.size()); }
	inline int idle_size() { return nWaiting; }
	inline std::thread &get_thread(int i) { return *threads[i]; }

	void resize(int nThreads);
	void stop(bool isWait);

	template <typename F, typename... Rest>
	std::future<typename std::result_of<F(Rest...)>::type> push(F &&f, Rest &&... rest)
	{
		auto tsk = std::make_shared<std::packaged_task<decltype(f(rest...))()>>(
			std::bind(std::forward<F>(f), std::forward<Rest>(rest)...));
		taskQueue.push(new Task([tsk]() { (*tsk)(); }));

		Lock lock(mutex);
		cv.notify_one();

		return tsk->get_future();
	}

	inline Task pop()
	{
		Task *t = nullptr;
		taskQueue.pop(t);
		if (t)
			return std::move(*t);
		return nullptr;
	}

	inline void clear_task_queue()
	{
		Task *t;
		while (taskQueue.pop(t))
			delete t;
	}

  private:
	void set_thread(int i);
};

} // namespace eve
