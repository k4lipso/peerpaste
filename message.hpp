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

    bool decode_header()
    {
        //SERIALIZATION Apache Thrift, Cap'n Proto, FlatBuffers
        //https://stackoverflow.com/questions/8178802/where-to-implement-a-protocol-using-boostasio
        const auto data = data_to_string();
        std::cout << "decode: " << data << std::endl;
        m_header.version = data.substr(0, 2);
        m_header.t_flag = (bool) data[3];
        m_header.request_type = data.substr(16, 23);
        m_header.ttl = std::stoi(data.substr(24, 31));
        m_header.message_length = std::stoull(data.substr(32, 63));
        m_header.transaction_id = data.substr(63, 83);
        m_header.printHeader();

        return true;
    }

    char handle_body();

    void encode_header();

private:

    bool isGet(const char b)
    {
        return false;
    }

    bool isHeaderValid(char header[])
    {
        return true;
    }

    struct CommonHeader {
        bool t_flag;
        short ttl;
        unsigned int message_length;
        std::string version;
        std::string request_type;
        std::string transaction_id;

        void printHeader()
        {
            std::cout << "Version: " << version << std::endl;
            std::cout << "T Flag: " << t_flag << std::endl;
            std::cout << "Request Type: " << request_type << std::endl;
            std::cout << "TTL: " << ttl << std::endl;
            std::cout << "Message Length: " << message_length << std::endl;
            std::cout << "Transaction ID: " << transaction_id << std::endl;
        }
    };

    //Dynamic Stream buffer, converted to string when needed
    boost::asio::streambuf request;
    std::ostream request_stream;

    CommonHeader m_header;
};

#endif // MESSAGE_HPP

