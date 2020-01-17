#include "peerpaste/threadpool.hpp"

ThreadPool::ThreadPool(unsigned thread_count) : done_(false)
{
	thread_count = thread_count ? thread_count : std::thread::hardware_concurrency();
	for(unsigned i = 0; i < thread_count; ++i)
	{
		threads_.reserve(thread_count);
		threads_.push_back(std::thread(&ThreadPool::worker_thread, this));
	}
}

ThreadPool::~ThreadPool()
{
	done_ = true;
}

void ThreadPool::worker_thread()
{
	while(!done_)
	{
		std::function<void()> task;
		if(work_queue_.try_pop(task))
		{
			task();
		}
		else
		{
			std::this_thread::yield();
		}
	}
}
