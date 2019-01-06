#include "header.hpp"
#include "peer.hpp"
#include "message.hpp"
#include "message_converter.hpp"
#include "cryptowrapper.hpp"

#include <iostream>
#include <string>
#include <memory>

#include <boost/asio.hpp>
#include <boost/array.hpp>

#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include <catch.hpp>

TEST_CASE("Testing Header Object", "[peerpaste::message::header]")
{
    bool t_flag = true;
    uint32_t ttl = 1;
    uint64_t message_length = 10;
    std::string request_type = "query";
    std::string transaction_id = "123ABC";
    std::string version = "1.0.0";
    std::string response_code = "UNKNOWN";

    Header header(t_flag, ttl, message_length, request_type,
                  transaction_id, version, response_code);

    REQUIRE(header.get_t_flag() == true);
    REQUIRE(header.get_ttl() == 1);
    REQUIRE(header.get_message_length() == 10);
    REQUIRE(header.get_request_type() == "query");
    REQUIRE(header.get_transaction_id() == "123ABC");
    REQUIRE(header.get_version() == "1.0.0");
    REQUIRE(header.get_response_code() == "UNKNOWN");

    header.set_t_flag(false);
    header.set_ttl(2);
    header.set_message_length(11);
    header.set_request_type("join");
    header.set_transaction_id("456DEF");
    header.set_version("2.0.1");
    header.set_response_code("KNOWN");

    REQUIRE(header.get_t_flag() == false);
    REQUIRE(header.get_ttl() == 2);
    REQUIRE(header.get_message_length() == 11);
    REQUIRE(header.get_request_type() == "join");
    REQUIRE(header.get_transaction_id() == "456DEF");
    REQUIRE(header.get_version() == "2.0.1");
    REQUIRE(header.get_response_code() == "KNOWN");
}

TEST_CASE("Testing Peer Object", "[peerpaste::message::peer]")
{
    std::string id = "123ABC";
    std::string ip = "10.0.0.1";
    std::string port = "1337";

    Peer peer(id, ip, port);

    REQUIRE(peer.get_id() == "123ABC");
    REQUIRE(peer.get_ip() == "10.0.0.1");
    REQUIRE(peer.get_port() == "1337");

    peer.set_id("456DEF");
    peer.set_ip("20.2.2.2");
    peer.set_port("4242");

    REQUIRE(peer.get_id() == "456DEF");
    REQUIRE(peer.get_ip() == "20.2.2.2");
    REQUIRE(peer.get_port() == "4242");
}

TEST_CASE("Testing Message Object", "[peerpaste::message]")
{
    std::string id = "123ABC";
    std::string ip = "10.0.0.1";
    std::string port = "1337";

    Peer peer(id, ip, port);

    bool t_flag = true;
    uint32_t ttl = 1;
    uint64_t message_length = 10;
    std::string request_type = "query";
    std::string transaction_id = "123ABC";
    std::string version = "1.0.0";
    std::string response_code = "UNKNOWN";

    Header header(t_flag, ttl, message_length, request_type,
                  transaction_id, version, response_code);

    Message message;
    message.set_header(header);
    message.add_peer(peer);
    message.add_peer(peer);
    message.add_peer(peer);
    message.print();

    auto header_ = message.get_header();

    REQUIRE(header_.get_t_flag() == true);
    REQUIRE(header_.get_ttl() == 1);
    REQUIRE(header_.get_message_length() == 10);
    REQUIRE(header_.get_request_type() == "query");
    REQUIRE(header_.get_transaction_id() == "123ABC");
    REQUIRE(header_.get_version() == "1.0.0");
    REQUIRE(header_.get_response_code() == "UNKNOWN");

    auto peers = message.get_peers();

    REQUIRE(peers.size() == 3);
    std::vector<Peer> new_peers;
    for(auto peer : peers){
        REQUIRE(peer.get_id() == "123ABC");
        REQUIRE(peer.get_ip() == "10.0.0.1");
        REQUIRE(peer.get_port() == "1337");

        peer.set_id("456DEF");
        peer.set_ip("20.2.2.2");
        peer.set_port("4242");
        new_peers.push_back(peer);
    }

    message.set_peers(new_peers);
    for(const auto peer : message.get_peers()){
        REQUIRE(peer.get_id() == "456DEF");
        REQUIRE(peer.get_ip() == "20.2.2.2");
        REQUIRE(peer.get_port() == "4242");
    }
}

TEST_CASE( "Testing ProtobufMessageConverter", "[peeraste::MessageConverter]" )
{
    ProtobufMessageConverter converter;
}

TEST_CASE( "Testing util::between()", "[util::between()]" )
{
    std::string a = "a";
    std::string b = "b";
    std::string c = "c";
    std::string num1 = "1";
    std::string num2 = "2";
    std::string num3 = "3";

    REQUIRE(util::between(a, b, c) == true);
    REQUIRE(util::between(b, b, c) == false);
    REQUIRE(util::between(c, b, c) == true);
    REQUIRE(util::between(a, b, a) == true);
    REQUIRE(util::between(c, a, b) == true);
    REQUIRE(util::between(num2, b, num1) == true);
    REQUIRE(util::between(num2, b, num3) == false);
    REQUIRE(util::between(num1, num2, num3) == true);
    REQUIRE(util::between(num3, num2, num1) == false);
    REQUIRE(util::between(num3, c, num2) == true);
    REQUIRE(util::between(c, num1, a) == true);
    REQUIRE(util::between(num1, num3, c) == true);
}
