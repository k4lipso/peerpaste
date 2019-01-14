#ifndef WRITE_QUEUE_HPP
#define WRITE_QUEUE_HPP

#include "message.hpp"
#include "request_object.hpp"

#include <deque>

class Session;

class WriteQueue
{
public:
    typedef std::shared_ptr<Message> MessagePtr;
    typedef std::shared_ptr<Session> SessionPtr;
    typedef std::shared_ptr<RequestObject> RequestObjectPtr;

    WriteQueue(WriteQueue const&) = delete;
    void operator=(WriteQueue const&) = delete;

    static std::shared_ptr<WriteQueue> GetInstance();
    void push_back(const RequestObjectPtr request);
    const RequestObjectPtr front() const;
    void pop_front() noexcept;
    const size_t size() const noexcept;
    const bool empty() const noexcept;
private:
    WriteQueue();

    std::deque<RequestObjectPtr> deque_;
};
#endif /* ifndef WRITE_QUEUE_HPP */
