#pragma once

#include <thread>
#include <vector>
#include <atomic>
#include <functional>
#include <type_traits>

#include "peerpaste/concurrent_queue.hpp"

class ThreadPool
{
public:
	ThreadPool(unsigned thread_count = 0);
	~ThreadPool();

	template<typename FunctionType>
	void submit(std::shared_ptr<FunctionType> function)
	{
		work_queue_.push_new(std::bind(&FunctionType::operator(),
																	 function));
	}

	template<typename FunctionType>
	void submit(FunctionType&& function)
	{
		using BareFunctionType = std::remove_cv_t<std::remove_reference_t<FunctionType>>;
		work_queue_.push_new(std::bind(&BareFunctionType::operator(),
																	 std::make_shared<BareFunctionType>(std::forward<FunctionType>(function))));
	}

private:
  void worker_thread();

  std::atomic<bool> done_;
  std::vector<std::thread> threads_;
  peerpaste::ConcurrentQueue<std::function<void()>> work_queue_;
};
