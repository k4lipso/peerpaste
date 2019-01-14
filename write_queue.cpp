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
        deque_.push_back(request);
    }

    const RequestObjectPtr WriteQueue::front() const
    {
        return deque_.front();
    }

    void WriteQueue::pop_front() noexcept
    {
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
