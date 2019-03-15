#include "message_queue.hpp"
#include "session.hpp"

#include <deque>

    typedef std::shared_ptr<Message> MessagePtr;
    typedef std::shared_ptr<Session> SessionPtr;
    typedef std::shared_ptr<RequestObject> RequestObjectPtr;

    std::shared_ptr<MessageQueue> MessageQueue::GetInstance()
    {
        static std::shared_ptr<MessageQueue> instance { new MessageQueue };
        return instance;
    }

    void MessageQueue::push_back(const MessagePtr message, const SessionPtr session)
    {
        std::lock_guard<std::mutex> guard(mutex_);
        auto request = std::make_shared<RequestObject>();
        request->set_message(message);
        request->set_connection(session);
        deque_.push_back(request);
    }

    void MessageQueue::push_back(const RequestObjectPtr request_object)
    {
        std::lock_guard<std::mutex> guard(mutex_);
        deque_.push_back(request_object);
    }

    const RequestObjectPtr MessageQueue::front()
    {
        std::lock_guard<std::mutex> guard(mutex_);
        std::cout << "FRONT ON SIZE: " << deque_.size() << std::endl;
        return deque_.front();
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
