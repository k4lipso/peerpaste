#ifndef MESSAGE_QUEUE_HPP
#define MESSAGE_QUEUE_HPP
#include "message.hpp"

#include <deque>

class Session;

class MessageQueue
{
public:
    typedef std::shared_ptr<Message> MessagePtr;
    typedef std::shared_ptr<Session> SessionPtr;

    MessageQueue(MessageQueue const&) = delete;
    void operator=(MessageQueue const&) = delete;

    static std::shared_ptr<MessageQueue> GetInstance();
    void push_back(const MessagePtr message, const SessionPtr session);
    const std::pair<MessagePtr, SessionPtr> front() const;
    void pop_front() noexcept;
    const size_t size() const noexcept;
    const bool empty() const noexcept;
private:
    MessageQueue();

    std::deque<std::pair<MessagePtr, SessionPtr>> deque_;
};
#endif /* ifndef MESSAGE_QUEUE_HPP */
