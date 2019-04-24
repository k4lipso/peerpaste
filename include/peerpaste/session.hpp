#ifndef SESSION_HPP
#define SESSION_HPP

#include <iostream>
#include <memory>
#include <string>

using boost::asio::ip::tcp;
using DataBuffer = std::vector<boost::uint8_t>;

//Forward declaration
class MessageQueue;

class Session
    : public std::enable_shared_from_this<Session>
{
public:
    typedef std::shared_ptr<Session> SessionPtr;
    typedef std::shared_ptr<Message> MessagePtr;

    Session (std::shared_ptr<peerpaste::ConcurrentQueue<std::pair<std::vector<uint8_t>,
                                                                  SessionPtr>>> msg_queue);
    virtual ~Session ();
    virtual void write(const DataBuffer& encoded_message) = 0;
    virtual void write_to(const DataBuffer& encoded_message,
                          const std::string& address,
                          const std::string& port) = 0;
    virtual void read() = 0;

private:
    std::shared_ptr<peerpaste::ConcurrentQueue<std::pair<std::vector<uint8_t>,
                                                         SessionPtr>>> msg_queue_;

};

#endif /* ifndef SESSION_HPP */
