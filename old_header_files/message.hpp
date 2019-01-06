#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <memory>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <functional>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>

#include "proto/messages.pb.h"
#include <google/protobuf/util/delimited_message_util.h>

typedef std::vector<boost::uint8_t> data_buffer;
const unsigned HEADER_SIZE = 4;

template <class MessageType>
class PackedMessage
{
public:
    typedef std::shared_ptr<MessageType> MessagePtr;

    PackedMessage(MessagePtr msg = MessagePtr())
        : m_msg(msg)
    {}

    void set_msg(MessagePtr msg)
    {
        m_msg = msg;
    }

    MessagePtr get_msg()
    {
        return m_msg;
    }

    size_t size() {
        return 0;
    }

    bool pack(data_buffer& buf) const
    {
        if(!m_msg)
            return false;

        unsigned msg_size = m_msg->ByteSize();
        buf.resize(HEADER_SIZE + msg_size);
        encode_header(buf, msg_size);
        return m_msg->SerializeToArray(&buf[HEADER_SIZE], msg_size);
    }

    unsigned decode_header(const data_buffer& buf) const
    {
        if(buf.size() < HEADER_SIZE)
            return 0;
        unsigned msg_size = 0;
        for(unsigned i = 0; i < HEADER_SIZE; ++i){
            msg_size = msg_size * 256 + (static_cast<unsigned>(buf[i]) & 0xFF);
        }
        return msg_size;
    }

    bool unpack(const data_buffer& buf)
    {
        BOOST_LOG_TRIVIAL(debug) << "unpack()";
        return m_msg->ParseFromArray(&buf[HEADER_SIZE], buf.size() - HEADER_SIZE);
    }

    bool init_header(const bool t_flag,
                     const uint32_t ttl,
                     const uint64_t message_length,
                     const std::string request_type,
                     const std::string transaction_id,
                     const std::string version )
    {
        auto header = m_msg->mutable_commonheader();
        header->set_t_flag(t_flag);
        header->set_ttl(ttl);
        header->set_message_length(message_length);
        header->set_request_type(request_type);
        header->set_transaction_id(transaction_id);
        header->set_version(version);

        return true;
    }



private:

    void encode_header(data_buffer& buf, unsigned size) const
    {
        assert(buf.size() >= HEADER_SIZE);
        buf[0] = static_cast<boost::uint8_t>((size >> 24) & 0xFF);
        buf[1] = static_cast<boost::uint8_t>((size >> 16) & 0xFF);
        buf[2] = static_cast<boost::uint8_t>((size >> 8) & 0xFF);
        buf[3] = static_cast<boost::uint8_t>(size & 0xFF);
    }

    MessagePtr m_msg;
};

#endif // MESSAGE_HPP
