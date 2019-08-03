#ifndef CONCURRENT_QUEUE
#define CONCURRENT_QUEUE

#include <mutex>
#include <queue>
#include <memory>
#include <condition_variable>
#include <chrono>

namespace peerpaste {
using namespace std::chrono_literals;

/*
 * ConcurrentQueue
 * Boost::Asio Session will push the received messages onto it
 * and a polling consumer will dispatch them to different performers
 */
template<typename T>
class ConcurrentQueue
{
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<std::unique_ptr<T>> queue_;
public:
    ConcurrentQueue()
    {}

    void wait_and_pop(T& value)
    {
        std::unique_lock lk(mutex_);
        condition_.wait(lk, [this]{ return not queue_.empty(); });
        value = std::move(*queue_.front());
        queue_.pop();
    }

    std::unique_ptr<T> wait_and_pop()
    {
        std::unique_lock lk(mutex_);
        condition_.wait(lk, [this]{ return not queue_.empty(); });
        auto result = std::move(queue_.front());
        queue_.pop();
        return result;
    }

    std::unique_ptr<T> wait_for_and_pop(int dur = 1)
    {
        std::chrono::duration<int> dur_ = std::chrono::seconds(dur);
        std::unique_lock lk(mutex_);
        if(condition_.wait_for(lk, dur_, [this]{ return not queue_.empty(); })){
            auto result = std::move(queue_.front());
            queue_.pop();
            return result;
        }
        return nullptr;
    }

    bool empty() const
    {
        std::scoped_lock lk(mutex_);
        return queue_.empty();
    }

    void push(T new_value)
    {
        /* auto new_value_ptr = std::make_unique<T>(std::move(new_value)); */
        std::scoped_lock lk(mutex_);
        queue_.push(std::make_unique<T>(std::move(new_value)));
        condition_.notify_one();
    }
};

} // closing peerpaste namespace
#endif /* ifndef CONCURRENT_QUEUE */
