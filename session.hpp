#ifndef SESSION_H
#define SESSION_H

#include "message.hpp"

#include <string>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class session
    : public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket)
        : socket_(std::move(socket))
    {
        std::cout << "SESSION CREATED" << std::endl;
    }

    void start()
    {
        std::cout << "Session started" << std::endl;
        do_read_header();
    }

private:
    void do_read_header()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                *m_message.data(), boost::asio::transfer_exactly(84),
                [this, self](boost::system::error_code ec, std::size_t)
                {
                    if(!ec){
                        m_message.decode_header();
                        do_read_header();
                    }
                });
    }

    void find_successor(/*ID*/);

    void check_precending_node(/*ID*/);

    void do_read_body();

    std::string Put(int level_of_decryption);

    std::string Get(size_t hash);

    void do_write(std::size_t length);

    Message m_message;
    tcp::socket socket_;
    /* char data_[paste::header_length + paste::max_body_length]; */
    /* std::string hash_; */
    /* char msg_[paste::max_body_length]; */
};

#endif /* SESSION_H */
