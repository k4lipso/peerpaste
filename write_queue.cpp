#include "write_queue.hpp"
#include "session.hpp"

#include <deque>

    typedef std::shared_ptr<Message> MessagePtr;
    typedef std::shared_ptr<Session> SessionPtr;
    typedef std::shared_ptr<RequestObject> RequestObjectPtr;

    std::shared_ptr<WriteQueue> WriteQueue::GetInstance()
    {
        static std::shared_ptr<WriteQueue> instance { new WriteQueue };
        return instance;
    }

    void WriteQueue::push_back(const RequestObjectPtr request)
    {
        std::lock_guard<std::mutex> guard(mutex_);
        deque_.push_back(request);
    }

    const RequestObjectPtr WriteQueue::front() const
    {
        std::lock_guard<std::mutex> guard(mutex_);
        return deque_.front();
    }

    void WriteQueue::pop_front()
    {
        std::lock_guard<std::mutex> guard(mutex_);
        deque_.pop_front();
    }

    const size_t WriteQueue::size() const noexcept
    {
        return deque_.size();
    }

    const bool WriteQueue::empty() const noexcept
    {
        return deque_.empty();
    }

    WriteQueue::WriteQueue()
    {}
