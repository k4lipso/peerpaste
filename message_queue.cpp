#include "session.hpp"
#include "message.hpp"

#include <deque>

class MessageQueue
{
public:
    typedef std::shared_ptr<Message> MessagePtr;
    typedef std::shared_ptr<Session> SessionPtr;

    static std::shared_ptr<MessageQueue> GetInstance()
    {
        static std::shared_ptr<MessageQueue> instance { new MessageQueue };
        return instance;
    }

    MessageQueue(MessageQueue const&) = delete;
    void operator=(MessageQueue const&) = delete;

    void push_back(const MessagePtr message, const SessionPtr session)
    {
        deque_.push_back(std::make_pair(message, session));
    }

    const std::pair<MessagePtr, SessionPtr> front() const
    {
        return deque_.front();
    }

    void pop_front() noexcept
    {
        deque_.pop_front();
    }

    const size_t size() const
    {
        return deque_.size();
    }

private:
    MessageQueue()
    {}

    std::deque<std::pair<MessagePtr, SessionPtr>> deque_;
};
