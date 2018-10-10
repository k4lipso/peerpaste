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
        BOOST_LOG_TRIVIAL(info) << "SESSION CREATED";
    }

    void start()
    {
        BOOST_LOG_TRIVIAL(info) << "Session started";
        do_read_varint();
    }

private:
    void do_read_varint()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                //Read 1 Byte which should be an Varint
                *m_message.data(), boost::asio::transfer_exactly(1),
                [this, self](boost::system::error_code ec, std::size_t)
                {
                    if(!ec){
                        uint32_t size;
                        if(!m_message.get_varint(size))
                        {
                            do_read_varint();
                            return;
                        }

                        //everything worked, read body using the size extraced from the varint
                        do_read_message(size);
                        return;
                    }

                    do_read_varint();
                });
    }

    void find_successor(/*ID*/);

    void check_precending_node(/*ID*/);

    void do_read_message(const uint32_t& size)
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                *m_message.data(), boost::asio::transfer_exactly(size),
                [this, self, size](boost::system::error_code ec, std::size_t)
                {
                    if(!ec){
                        if(!m_message.decode_request(size))
                        {
                            do_read_varint();
                            return;
                        }
                        //TODO: Change method to accept further message objects
                        //other than common header,
                        //a message evaluation chain has do be developed to receive
                        //a sequence of different objects depending on the
                        //common_header request_tpe
                        //
                        //idea is to create request/response objects using protobuf so
                        //that only one object has to be transmitted each time
                        /* do_read_objects(m_message.get_message_length()); */
                        std::cout << "And NOW???" << std::endl;
                        std::cout << "Request Type: " << m_message.get_request_type() << std::endl;

                        if(!handle_message())
                        {
                            do_read_varint();
                            return;
                        }

                        send_response();
                    }

                });
    }

    bool handle_message()
    {
        //determine request type
        auto request_type = m.message.get_request_type();
        //handle request type
        if(request_type == "query")
        {
            if(!handle_query()) return false;
        } else {
            BOOST_LOG_TRIVIAL(debug) << "Unknown Request Type";
            return false;
        }

        return true;
    }

    bool handle_query()
    {

    }

    bool send_response()
    {
        return false;
    }

    void do_read_common_header(const uint32_t& size)
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                *m_message.data(), boost::asio::transfer_exactly(size),
                [this, self, size](boost::system::error_code ec, std::size_t)
                {
                    if(!ec){
                        if(!m_message.decode_header(size))
                        {
                            do_read_varint();
                            return;
                        }
                        //TODO: Change method to accept further message objects
                        //other than common header,
                        //a message evaluation chain has do be developed to receive
                        //a sequence of different objects depending on the
                        //common_header request_tpe
                        //
                        //idea is to create request/response objects using protobuf so
                        //that only one object has to be transmitted each time
                        do_read_objects(m_message.get_message_length());
                    }

                });
    }

    void do_read_objects(const uint32_t& size)
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                *m_message.data(), boost::asio::transfer_exactly(size),
                [this, self, size](boost::system::error_code ec, std::size_t)
                {
                    if(!ec){
                        if(!m_message.decode_peerinfo(size))
                        {
                            do_read_varint();
                            return;
                        }
                        std::cout << m_message.data_to_string() << std::endl;
                        if(!m_message.decode_peerinfo(size))
                        {
                            do_read_varint();
                            return;
                        }
                        //TODO: Change method to accept further message objects
                        //other than common header
                        do_read_varint();
                    }

                });
    }

    //TODO: think about holding some std::vector<std::shared_ptr<google::protobuf::MessageLite>>
    //which is used to set the order of objects to read

    std::string Put(int level_of_decryption);

    std::string Get(size_t hash);

    void do_write(std::size_t length);

    Message m_message;
    tcp::socket socket_;
};

#endif /* SESSION_H */
