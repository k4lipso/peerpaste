#include "peerpaste/message_queue.hpp"
#include "peerpaste/session.hpp"

#include <deque>

std::shared_ptr<MessageQueue> MessageQueue::GetInstance()
{
    static std::shared_ptr<MessageQueue> instance { new MessageQueue };
    return instance;
}

void MessageQueue::push_back(const MessagePtr message, const SessionPtr session)
{
    std::lock_guard<std::mutex> guard(mutex_);
    auto request = std::make_unique<RequestObject>();
    request->set_message(message);
    request->set_connection(session);
    deque_.push_back(std::move(request));
}

void MessageQueue::push_back(RequestObjectUPtr&& request_object)
{
    std::lock_guard<std::mutex> guard(mutex_);
    deque_.push_back(std::move(request_object));
}

const RequestObjectUPtr MessageQueue::front()
{
    std::lock_guard<std::mutex> guard(mutex_);
    return std::move(deque_.front());
}

void MessageQueue::pop_front() noexcept
{
    std::lock_guard<std::mutex> guard(mutex_);
    if(deque_.size() != 0){
        deque_.pop_front();
    }
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
