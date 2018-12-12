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


/**
 * The message object represents an actual message
 * and some extra Information and Methods.
 * It will contain Methods to Crypt and Decrypt the message.
 */
class Message
{
public:
    /*  Common Header - Length 84 Bits
     *   0                   1                   2                   3
     *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *  | V=3 |T|       Reserved        | Request Type  |      TTL      |
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *  |                       Message Length                          |
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *  |                       Transaction-ID                          |
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     */

    Message()
        : m_input_stream(&m_input),
          m_output_stream(&m_output)
    {
    }

    boost::asio::streambuf* data()
    {
        return &m_input;
    }

    boost::asio::streambuf* output()
    {
        return &m_output;
    }

    std::ostream* output_stream()
    {
        return &m_output_stream;
    }

    const uint64_t get_message_length()
    {
        return m_header.message_length();
    }

    bool get_varint(uint32_t& size)
    {
        //Convert message into CodedInputStream for reading varint
        //TODO: eva if google (CodedInputStream) independent code is possible
        boost::asio::streambuf::const_buffers_type bufs = m_input.data();
        boost::asio::streambuf::const_buffers_type bufs2 = m_input.data();
        std::string str2(boost::asio::buffers_begin(bufs2),
                         boost::asio::buffers_begin(bufs2) + m_input.size());
        std::cout << str2 << std::endl;

        std::istream reader(&m_input);
        google::protobuf::io::IstreamInputStream zstream(&reader);
        google::protobuf::io::CodedInputStream coded_input(&zstream);

        //Try to read Varint, if fails start all over again
        if(!coded_input.ReadVarint32(&size))
        {
            /* boost::asio::streambuf source, target; */
/* ... */
/* std::size_t bytes_copied = buffer_copy( */
  /* target.prepare(source.size()), // target's output sequence */
  /* source.data());                // source's input sequence */
/* // Explicitly move target's output sequence to its input sequence. */
/* target.commit(bytes_copied); */

            m_input.commit(buffer_copy(
                                    m_input.prepare(bufs.size()), // target's output sequence
                                    m_input.data()));

            BOOST_LOG_TRIVIAL(error) << "ReadVarint32(&size) failed";
            return false;
        }

        BOOST_LOG_TRIVIAL(info) << "Varint: " << size;
        return true;
    }

    const std::string get_request_type() const
    {
        return m_header.request_type();
    }

    const std::string get_peer_id() const
    {
        return m_request.peerinfo(0).peer_id();
    }

    const std::shared_ptr<Request> get_Message() const
    {
        return std::make_shared<Request>(m_request);
    }

    const bool get_t_flag() const
    {
        return m_header.t_flag();
    }

    bool decode_message(uint32_t size)
    {
        //Convert message into CodedInputStream for reading varint
        std::istream reader(data());
        google::protobuf::io::IstreamInputStream zstream(&reader);
        google::protobuf::io::CodedInputStream coded_input(&zstream);

        //Limit the CodedInputStream
        google::protobuf::io::CodedInputStream::Limit limit = coded_input.PushLimit(size);

        //Merge Stream into CommonHeader Object
        if(!m_request.MergeFromCodedStream(&coded_input))
        {
            BOOST_LOG_TRIVIAL(error) << "MergeFromCodedStream Failed";
            return false;
        }

        if(!coded_input.ConsumedEntireMessage())
        {
            BOOST_LOG_TRIVIAL(error) << "ConsumedEntireMessage returned false";
            return false;
        }

        coded_input.PopLimit(limit);

        m_header = m_request.commonheader();

        //Print CommanHeader Information
        log_common_header(m_header);

        return true;
    }

    //Decodes streambuf into a Common Header Object if possible
    //TODO: Maybe pass CommonHeader*
    bool decode_header(uint32_t size)
    {
        //Convert message into CodedInputStream for reading varint
        std::istream reader(data());
        google::protobuf::io::IstreamInputStream zstream(&reader);
        google::protobuf::io::CodedInputStream coded_input(&zstream);

        //Limit the CodedInputStream
        google::protobuf::io::CodedInputStream::Limit limit = coded_input.PushLimit(size);

        //Merge Stream into CommonHeader Object
        if(!m_header.MergeFromCodedStream(&coded_input))
        {
            BOOST_LOG_TRIVIAL(error) << "MergeFromCodedStream Failed";
            return false;
        }

        if(!coded_input.ConsumedEntireMessage())
        {
            BOOST_LOG_TRIVIAL(error) << "ConsumedEntireMessage returned false";
            return false;
        }

        coded_input.PopLimit(limit);

        //Print CommanHeader Information
        log_common_header(m_header);

        return true;
    }

    bool decode_peerinfo(uint32_t size)
    {
        PeerInfo peerinfo;
        peerinfo.set_peer_id("Foo");

        //TODO: this deletes all data from input_stream.
        //if 2 Message Object are Cached only the first one can be merged
        std::istream reader(data());
        google::protobuf::io::IstreamInputStream zstream(&reader);
        bool clean_eof;

        if(!google::protobuf::util::ParseDelimitedFromZeroCopyStream(&peerinfo, &zstream, &clean_eof))
        {
            BOOST_LOG_TRIVIAL(error) << "ParseDelimitedFromZeroCopyStream failed";
            return false;
        }

        BOOST_LOG_TRIVIAL(info) << "PeerInfo PeerID: " << peerinfo.peer_id();
        return true;
    }

    void log_request(const Request& request)
    {
        log_common_header(request.commonheader());
    }

    void log_common_header(const CommonHeader& common_header)
    {
        BOOST_LOG_TRIVIAL(info) << "START OBJECT COMMONDHEADER:";
        BOOST_LOG_TRIVIAL(info) << "\t" << "T Flag: " << common_header.t_flag();
        BOOST_LOG_TRIVIAL(info) << "\t" << "TTL: " << common_header.ttl();
        BOOST_LOG_TRIVIAL(info) << "\t" << "Message Length: " << common_header.message_length();
        BOOST_LOG_TRIVIAL(info) << "\t" << "Request Type: " << common_header.request_type();
        BOOST_LOG_TRIVIAL(info) << "\t" << "Transaction ID: " << common_header.transaction_id();
        BOOST_LOG_TRIVIAL(info) << "\t" << "Version: " << common_header.version();
        BOOST_LOG_TRIVIAL(info) << "STOP OBJECT COMMONHEADER";
    }

    char handle_body();

    //TODO: return shrd ptr?
    bool set_common_header(CommonHeader* header,
                                    const bool t_flag,
                                    const uint32_t ttl,
                                    const uint64_t message_length,
                                    const std::string request_type,
                                    const std::string transaction_id,
                                    const std::string version )
    {
        header->set_t_flag(t_flag);
        header->set_ttl(ttl);
        header->set_message_length(message_length);
        header->set_request_type(request_type);
        header->set_transaction_id(transaction_id);
        header->set_version(version);

        return true;
    }

    //TODO: Provide interface where own streambuf is passed by ref or ptr
    bool encode_common_header(const bool t_flag,
                              const uint32_t ttl,
                              const uint64_t message_length,
                              const std::string request_type,
                              const std::string transaction_id,
                              const std::string version )
    {
        CommonHeader header;
        header.set_t_flag(t_flag);
        header.set_ttl(ttl);
        header.set_message_length(message_length);
        header.set_request_type(request_type);
        header.set_transaction_id(transaction_id);
        header.set_version(version);

        if(!google::protobuf::util::SerializeDelimitedToOstream(header,
                                                               &m_output_stream))
        {
            BOOST_LOG_TRIVIAL(error) << "encode_common_header(..) SerializeDelimitedToOstream failed";
            return false;
        }

        return true;
    }

    bool serialize_object(const google::protobuf::MessageLite& msg){
        if(!google::protobuf::util::SerializeDelimitedToOstream(msg,
                                                               &m_output_stream))
        {
            BOOST_LOG_TRIVIAL(error) << "encode_peer_info(..) SerializeDelimitedToOstream failed";
            return false;
        }

        return true;
    }

    bool encode_peer_info(const std::string peer_id, uint32_t& size)
    {
        PeerInfo peerinfo;
        peerinfo.set_peer_id(peer_id);
        // + 1 for size of varint
        size = peerinfo.ByteSize() + 1;
        std::cout << "JOJO " << size << std::endl;
        if(!google::protobuf::util::SerializeDelimitedToOstream(peerinfo,
                                                               &m_output_stream))
        {
            BOOST_LOG_TRIVIAL(error) << "encode_peer_info(..) SerializeDelimitedToOstream failed";
            return false;
        }

        return true;
    }

    bool encode_peer_info(const std::string peer_id, std::ostream& output_stream)
    {
        PeerInfo peerinfo;
        peerinfo.set_peer_id(peer_id);
        if(!google::protobuf::util::SerializeDelimitedToOstream(peerinfo,
                                                               &output_stream))
        {
            BOOST_LOG_TRIVIAL(error) << "encode_peer_info(..) SerializeDelimitedToOstream failed";
            return false;
        }

        return true;
    }

    std::string data_to_string()
    {
        std::istream response_stream(&m_input);
        std::string data(std::istreambuf_iterator<char>(response_stream), {});
        return data;
    }

private:

    /**
     *  We define seven messages for service interface namely, join, leave, keep-alive,
     *  routing-peer-lookup, update, query, replicate
     *  and three messages for data interface namely, insert, lookup and remove.
     */
    enum MessageType { JOIN,
                       LEAVE,
                       KEEPALIVE,
                       ROUTINGPEERLOOKUP,
                       UPDATE,
                       QUERY,
                       REPLICATE };

    bool isGet(const char b)
    {
        return false;
    }

    bool isHeaderValid(char header[])
    {
        return true;
    }

    //Dynamic Stream buffer, converted to string when needed
    //Contains received data
    boost::asio::streambuf m_input;
    std::ostream m_input_stream;

    //Streambuffer containing data to send
    boost::asio::streambuf m_output;
    std::ostream m_output_stream;

    CommonHeader m_header;
    //TODO: Better naming for request, request object could also be a response for now oO
    Request m_request;
};

#endif // MESSAGE_HPP