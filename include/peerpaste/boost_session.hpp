#ifndef BOOSTSESSION_HPP
#define BOOSTSESSION_HPP

#include "peerpaste/message.hpp"
#include "peerpaste/concurrent_queue.hpp"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>

#include <iostream>
#include <memory>
#include <string>

using boost::asio::ip::tcp;
using DataBuffer = std::vector<boost::uint8_t>;

//Forward declaration
class MessageQueue;

class BoostSession
    : public std::enable_shared_from_this<BoostSession>
{
public:
    typedef std::shared_ptr<BoostSession> BoostSessionPtr;
    typedef std::shared_ptr<Message> MessagePtr;

    BoostSession (boost::asio::io_context& io_context,
                std::shared_ptr<peerpaste::ConcurrentQueue<std::pair<std::vector<uint8_t>,
                                                                     BoostSessionPtr>>> msg_queue);
    ~BoostSession ();
    boost::asio::ip::tcp::socket& get_socket();
    void write(const std::vector<uint8_t>& encoded_message);
    void write_to(const std::vector<uint8_t>& encoded_message, const std::string& address,
                                                               const std::string& port);
    void read();
    const std::string get_client_ip() const;
    const unsigned get_client_port() const;

private:
    void stop();
    void queue_message(const std::vector<uint8_t>& message);
    void start_packet_send();
    void packet_send_done(boost::system::error_code const & error);
    void handle_read_message(const boost::system::error_code& ec);
    void do_read_message(unsigned msg_len);
    void handle_read_header(const boost::system::error_code& error);
    //TODO: this function has to post to the read_strand_
    void do_read_header();
    unsigned decode_header(const DataBuffer& buf) const;
    void encode_header(DataBuffer& buf, unsigned size) const;

    boost::asio::io_service& service_;
    boost::asio::io_service::strand write_strand_;
    boost::asio::io_service::strand read_strand_;
    std::deque<std::vector<uint8_t>> send_packet_queue;
    std::vector<uint8_t> readbuf_;
    std::shared_ptr<peerpaste::ConcurrentQueue<std::pair<std::vector<uint8_t>,
                                                         BoostSessionPtr>>> msg_queue_;

    tcp::socket socket_;
    std::string name_;
    const size_t header_size_ = 4;
};

#endif /* ifndef BOOSTSESSION_HPP */
