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

    enum { header_length = 84 };

    Message()
        : request_stream(&request)
    {
    }

    boost::asio::streambuf* data()
    {
        return &request;
    }

    std::string data_to_string()
    {
        std::istream response_stream(&request);
        std::string data(std::istreambuf_iterator<char>(response_stream), {});
        return data;
    }

    bool get_varint(uint32_t& size)
    {
        //Convert message into CodedInputStream for reading varint
        //TODO: eva if google (CodedInputStream) independent code is possible
        std::istream reader(data());
        google::protobuf::io::IstreamInputStream zstream(&reader);
        google::protobuf::io::CodedInputStream coded_input(&zstream);

        //Try to read Varint, if fails start all over again
        if(!coded_input.ReadVarint32(&size))
        {
            BOOST_LOG_TRIVIAL(error) << "ReadVarint32(&size) failed";
            return false;
        }

        return true;

    }

    bool decode_header(uint32_t size)
    {
        //Create a CommonHeader object
        CommonHeader message1;

        //Convert message into CodedInputStream for reading varint
        std::istream reader(data());
        google::protobuf::io::IstreamInputStream zstream(&reader);
        google::protobuf::io::CodedInputStream coded_input(&zstream);

        //Limit the CodedInputStream
        google::protobuf::io::CodedInputStream::Limit limit = coded_input.PushLimit(size);

        if(!message1.MergeFromCodedStream(&coded_input))
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

        std::cout << "STAQRT" << std::endl;
        std::cout << message1.t_flag() << std::endl;
        std::cout << message1.ttl() << std::endl;
        std::cout << message1.message_length() << std::endl;
        std::cout << message1.request_type() << std::endl;
        std::cout << message1.transaction_id() << std::endl;
        std::cout << message1.version() << std::endl;
        std::cout << "STOP" << std::endl;

        return true;
        //SERIALIZATION Apache Thrift, Cap'n Proto, FlatBuffers
        //https://stackoverflow.com/questions/8178802/where-to-implement-a-protocol-using-boostasio
        /* const auto data = data_to_string(); */
        /* std::cout << "decode: " << data << std::endl; */
        /* const auto length = std::stoull(data); */


        /* m_header.t_flag = 0; */
        /* m_header.ttl = 0; */
        /* m_header.message_length = 0; */
        /* std::cout << sizeof(m_header.version) << std::endl; */
        /* std::cout << sizeof(m_header.message_length) << std::endl; */
        /* std::cout << sizeof(m_header.transaction_id) << std::endl; */
        /* std::cout << sizeof(m_header) << std::endl; */

        /* strcpy(m_header.version, data.substr(0,2).c_str()); */
        /* /1* m_header.version = data.substr(0, 2); *1/ */
        /* m_header.t_flag = (bool) data[3]; */
        /* strcpy(m_header.request_type, data.substr(16,23).c_str()); */
        /* /1* m_header.request_type = &data.substr(16, 23); *1/ */
        /* m_header.ttl = std::stoi(data.substr(24, 31)); */
        /* m_header.message_length = std::stoull(data.substr(32, 63)); */
        /* strcpy(m_header.transaction_id, data.substr(63,83).c_str()); */
        /* /1* m_header.transaction_id = data.substr(63, 83); *1/ */
        /* m_header.printHeader(); */

        /* return true; */
    }

    char handle_body();

    void encode_common_header(bool t_flag,
                              uint32_t ttl,
                              uint64_t message_length,
                              std::string request_type,
                              std::string transaction_id,
                              std::string version )
    {
        CommonHeader header;
        header.set_t_flag(t_flag);
        header.set_ttl(ttl);
        header.set_message_length(message_length);
        header.set_request_type(request_type);
        header.set_transaction_id(transaction_id);
        header.set_version(version);

        std::string encoded_str;
        if(header.SerializeToString(&encoded_str)){
            std::cout << encoded_str << std::endl;
        }
    }

private:

    bool isGet(const char b)
    {
        return false;
    }

    bool isHeaderValid(char header[])
    {
        return true;
    }

    /* struct CommonHeader { */
    /*     bool t_flag; */
    /*     uint8_t ttl; */
    /*     uint32_t message_length; */
    /*     char version[3]; */
    /*     char request_type[8]; */
    /*     char transaction_id[32]; */

    /*     const std::vector<boost::asio::const_buffer> to_buffer() const */
    /*     { */
    /*         std::vector<boost::asio::const_buffer> buf; */
    /*         buf.push_back(boost::asio::buffer(&t_flag, sizeof(t_flag))); */
    /*         buf.push_back(boost::asio::buffer(&ttl, sizeof(ttl))); */
    /*         buf.push_back(boost::asio::buffer(&message_length, sizeof(message_length))); */
    /*         buf.push_back(boost::asio::buffer(version, sizeof(version))); */
    /*         buf.push_back(boost::asio::buffer(request_type, sizeof(request_type))); */
    /*         buf.push_back(boost::asio::buffer(transaction_id, sizeof(transaction_id))); */

    /*         return buf; */
    /*     } */

        /* void printHeader() */
        /* { */
        /*     /1* std::cout << "Version: " << version << std::endl; *1/ */
        /*     /1* std::cout << "T Flag: " << t_flag << std::endl; *1/ */
        /*     /1* std::cout << "Request Type: " << request_type << std::endl; *1/ */
        /*     /1* std::cout << "TTL: " << ttl << std::endl; *1/ */
        /*     /1* std::cout << "Message Length: " << message_length << std::endl; *1/ */
        /*     /1* std::cout << "Transaction ID: " << transaction_id << std::endl; *1/ */
        /* } */
    /* }; */

    //Dynamic Stream buffer, converted to string when needed
    boost::asio::streambuf request;
    std::ostream request_stream;


    CommonHeader m_header;
};

#endif // MESSAGE_HPP

