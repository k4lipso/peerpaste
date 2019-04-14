#ifndef MESSAGE_QUEUE_HPP
#define MESSAGE_QUEUE_HPP

#include "message.hpp"
#include "request_object.hpp"

#include <deque>
#include <mutex>

class Session;

using MessagePtr = std::shared_ptr<Message>;
using SessionPtr = std::shared_ptr<Session>;
using RequestObjectUPtr = std::unique_ptr<RequestObject>;

class MessageQueue
{
public:
    MessageQueue(MessageQueue const&) = delete;
    void operator=(MessageQueue const&) = delete;

    static std::shared_ptr<MessageQueue> GetInstance();
    void push_back(const MessagePtr message, const SessionPtr session);
    void push_back(RequestObjectUPtr&& request_object);
    const RequestObjectUPtr front();
    void pop_front() noexcept;
    const size_t size() const noexcept;
    const bool empty() const noexcept;
private:
    MessageQueue();
    std::deque<RequestObjectUPtr> deque_;
    mutable std::mutex mutex_;
};
#endif /* ifndef MESSAGE_QUEUE_HPP */