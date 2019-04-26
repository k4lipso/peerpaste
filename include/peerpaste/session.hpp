#ifndef SESSION_HPP
#define SESSION_HPP

#include <iostream>
#include <memory>
#include <string>

//Forward declaration
class Session;

using DataBuffer = std::vector<uint8_t>;
using MessagePtr = std::shared_ptr<Message>;
using SessionPtr = std::shared_ptr<Session>;

class Session
{
public:

    virtual ~Session () {}
    virtual void write(const DataBuffer& encoded_message) = 0;
    virtual void write_to(const DataBuffer& encoded_message,
                          const std::string& address,
                          const std::string& port) = 0;
    virtual void read() = 0;
    virtual const std::string get_client_ip() const = 0;

protected:
    std::shared_ptr<peerpaste::ConcurrentQueue<std::pair<DataBuffer,
                                                         SessionPtr>>> msg_queue_;

};

#endif /* ifndef SESSION_HPP */
