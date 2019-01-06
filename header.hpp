#ifndef HEADER_H
#define HEADER_H

#include <string>
#include <iostream>

class Header
{
public:
    Header(){}

    Header(bool t_flag,
           uint32_t ttl,
           uint64_t message_length,
           std::string request_type,
           std::string transaction_id,
           std::string version,
           std::string response_code)
           : t_flag_(t_flag), ttl_(ttl),
             message_length_(message_length),
             request_type_(request_type),
             transaction_id_(transaction_id),
             version_(version),
             response_code_(response_code)
    {}

    const void print() const
    {
        std::cout << "t_flag: " << get_t_flag() << '\n';
        std::cout << "ttl: " << get_ttl() << '\n';
        std::cout << "message_length: " << get_message_length() << '\n';
        std::cout << "request_type: " << get_request_type() << '\n';
        std::cout << "transaction_id: " << get_transaction_id() << '\n';
        std::cout << "version: " << get_version() << '\n';
        std::cout << "response_code: " << get_response_code() << '\n';
    }

    const void set_t_flag(const bool& t_flag)
    {
        t_flag_ = t_flag;
    }

    bool get_t_flag() const
    {
        return t_flag_;
    }

    const void set_ttl(const uint32_t& ttl)
    {
        ttl_ = ttl;
    }

    uint32_t get_ttl() const
    {
        return ttl_;
    }

    const void set_message_length(const uint64_t& message_length)
    {
        message_length_ = message_length;
    }

    uint64_t get_message_length() const
    {
        return message_length_;
    }

    const void set_request_type(const std::string& request_type)
    {
        request_type_ = request_type;
    }

    std::string get_request_type() const
    {
        return request_type_;
    }

    const void set_transaction_id(const std::string& transaction_id)
    {
        transaction_id_ = transaction_id;
    }

    std::string get_transaction_id() const
    {
        return transaction_id_;
    }

    const void set_version(const std::string& version)
    {
        version_ = version;
    }

    std::string get_version() const
    {
        return version_;
    }

    const void set_response_code(const std::string& response_code)
    {
        response_code_ = response_code;
    }

    std::string get_response_code() const
    {
        return response_code_;
    }

private:
    bool t_flag_;
    uint32_t ttl_;
    uint64_t message_length_;
    std::string request_type_;
    std::string transaction_id_;
    std::string version_;
    std::string response_code_;
};

#endif /* HEADER_H */
