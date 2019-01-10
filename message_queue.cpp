#include "message_queue.hpp"
#include "session.hpp"

#include <deque>

    typedef std::shared_ptr<Message> MessagePtr;
    typedef std::shared_ptr<Session> SessionPtr;

    std::shared_ptr<MessageQueue> MessageQueue::GetInstance()
    {
        static std::shared_ptr<MessageQueue> instance { new MessageQueue };
        return instance;
    }

    void MessageQueue::push_back(const MessagePtr message, const SessionPtr session)
    {
        deque_.push_back(std::make_pair(message, session));
    }

    const std::pair<MessagePtr, SessionPtr> MessageQueue::front() const
    {
        return deque_.front();
    }

    void MessageQueue::pop_front() noexcept
    {
        deque_.pop_front();
    }

    const size_t MessageQueue::size() const noexcept
    {
        return deque_.size();
    }

    const bool MessageQueue::empty() const noexcept
    {
        return deque_.empty();
    }

    MessageQueue::MessageQueue()
    {}
