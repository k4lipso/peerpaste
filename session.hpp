#ifndef SESSION_H
#define SESSION_H

#include "message.hpp"
/* #include "proto/messages.pb.h" */

#include <string>
#include <boost/asio.hpp>
#include <google/protobuf/util/delimited_message_util.h>

using boost::asio::ip::tcp;

class session
    : public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket)
        : socket_(std::move(socket))
    {
        BOOST_LOG_TRIVIAL(info) << "SESSION CREATED";
    }

    void start()
    {
        BOOST_LOG_TRIVIAL(info) << "Session started";
        do_read_header();
    }

private:
    void do_read_header()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                //Read 1 Byte which is an Varint
                *m_message.data(), boost::asio::transfer_exactly(1),
                [this, self](boost::system::error_code ec, std::size_t)
                {
                    if(!ec){
                        uint32_t size;
                        if(!m_message.get_varint(size))
                        {
                            do_read_header();
                            return;
                        }

                        //everything worked, read body using the size extraced from the varint
                        do_read_body(size);
                    }
                });
    }

    void find_successor(/*ID*/);

    void check_precending_node(/*ID*/);

    void do_read_body(uint32_t& size)
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                *m_message.data(), boost::asio::transfer_exactly(size),
                [this, self, size](boost::system::error_code ec, std::size_t)
                {
                    if(!ec){
                        if(!m_message.decode_header(size))
                        {
                            do_read_header();
                            return;
                        }
                        //TODO: Change method to accept further message objects
                        //other than common header
                        do_read_header();
                    }

                });


    }

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
