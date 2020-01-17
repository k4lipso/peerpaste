#pragma once

#include <thread>
#include <vector>
#include <atomic>
#include <functional>

#include "peerpaste/concurrent_queue.hpp"

class ThreadPool
{
public:
	ThreadPool(unsigned thread_count = 0);
	~ThreadPool();

	template<typename FunctionType>
	void submit(FunctionType function)
	{
		work_queue_.push(std::function<void()>(function));
	}

private:
  void worker_thread();

  std::atomic<bool> done_;
  std::vector<std::thread> threads_;
  peerpaste::ConcurrentQueue<std::function<void()>> work_queue_;
};
