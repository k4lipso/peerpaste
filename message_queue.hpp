#ifndef MESSAGE_QUEUE_HPP
#define MESSAGE_QUEUE_HPP
#include "message.hpp"
#include "request_object.hpp"

#include <deque>
#include <mutex>

class Session;

class MessageQueue
{
public:
    using MessagePtr = std::shared_ptr<Message>;
    using SessionPtr = std::shared_ptr<Session>;
    using RequestObjectPtr = std::shared_ptr<RequestObject>;

    MessageQueue(MessageQueue const&) = delete;
    void operator=(MessageQueue const&) = delete;

    static std::shared_ptr<MessageQueue> GetInstance();
    void push_back(const MessagePtr message, const SessionPtr session);
    void push_back(const RequestObjectPtr request_object);
    const RequestObjectPtr front() const;
    void pop_front() noexcept;
    const size_t size() const noexcept;
    const bool empty() const noexcept;
private:
    MessageQueue();
    std::deque<RequestObjectPtr> deque_;
    mutable std::mutex mutex_;
};
#endif /* ifndef MESSAGE_QUEUE_HPP */
