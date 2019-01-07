#include "header.hpp"
#include "peer.hpp"
#include "message.hpp"
#include "message_converter.hpp"
#include "message_queue.cpp"
#include "routing_table.hpp"
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
    //Create a protobuf message
    auto protobuf_message = std::make_shared<Request>();

    //Add header
    auto protobuf_header = protobuf_message->mutable_commonheader();
    protobuf_header->set_t_flag(true);
    protobuf_header->set_ttl(10);
    protobuf_header->set_message_length(15);
    protobuf_header->set_request_type("query");
    protobuf_header->set_transaction_id("secret");
    protobuf_header->set_version("1.0.0");
    protobuf_header->set_response_code("UNKNOWN");

    //Add first peer
    auto protobuf_peerinfo1 = protobuf_message->add_peerinfo();
    protobuf_peerinfo1->set_peer_id("123ABC");
    protobuf_peerinfo1->set_peer_ip("123ABC");
    protobuf_peerinfo1->set_peer_port("123ABC");

    //Add second peer
    auto protobuf_peerinfo2 = protobuf_message->add_peerinfo();
    protobuf_peerinfo2->set_peer_id("321CBA");
    protobuf_peerinfo2->set_peer_ip("321CBA");
    protobuf_peerinfo2->set_peer_port("321CBA");

    //Create buffer and serialize protobuf message into it
    std::vector<uint8_t> buf;
    unsigned protobuf_message_size = protobuf_message->ByteSize();
    buf.resize(protobuf_message_size);
    protobuf_message->SerializeToArray(&buf[0], protobuf_message_size);

    //Create converter and create a Message from the given buffer
    ProtobufMessageConverter converter;
    auto peerpaste_message = converter.MessageFromSerialized(buf);

    //Check if header contains right information
    auto peerpaste_header = peerpaste_message->get_header();
    REQUIRE(peerpaste_header.get_t_flag() == true);
    REQUIRE(peerpaste_header.get_ttl() == 10);
    REQUIRE(peerpaste_header.get_message_length() == 15);
    REQUIRE(peerpaste_header.get_request_type() == "query");
    REQUIRE(peerpaste_header.get_transaction_id() == "secret");
    REQUIRE(peerpaste_header.get_version() == "1.0.0");
    REQUIRE(peerpaste_header.get_response_code() == "UNKNOWN");

    //Check if peers contain right information
    auto peerpaste_peers = peerpaste_message->get_peers();
    REQUIRE(peerpaste_peers.size() == 2);
    REQUIRE(peerpaste_peers[0].get_id() == "123ABC");
    REQUIRE(peerpaste_peers[0].get_ip() == "123ABC");
    REQUIRE(peerpaste_peers[0].get_port() == "123ABC");
    REQUIRE(peerpaste_peers[1].get_id() == "321CBA");
    REQUIRE(peerpaste_peers[1].get_ip() == "321CBA");
    REQUIRE(peerpaste_peers[1].get_port() == "321CBA");

    //Check if the created buffer is the same when generated by message_converter
    REQUIRE(buf == converter.SerializedFromMessage(peerpaste_message));
}

TEST_CASE( "Testing MessageQueue", "[]" )
{
    auto message = std::make_shared<Message>();
    auto session = std::make_shared<Session>();
    auto test_pair = std::make_pair(message, session);
    auto message_queue = MessageQueue::GetInstance();

    REQUIRE(message_queue->size() == 0);
    message_queue->push_back(message, session);
    REQUIRE(message_queue->size() == 1);
    REQUIRE(test_pair == message_queue->front());

    auto message_queue2 = MessageQueue::GetInstance();
    REQUIRE(message_queue2->size() == message_queue->size());

    message_queue->pop_front();
    REQUIRE(message_queue->size() == 0);
    REQUIRE(message_queue2->size() == 0);
}

TEST_CASE( "Testing RoutingTable", "[]" )
{
    auto peer1 = std::make_shared<Peer>();
    peer1->set_id("1");
    peer1->set_ip("1");
    peer1->set_port("1");

    auto peer2 = std::make_shared<Peer>();
    peer2->set_id("2");
    peer2->set_ip("2");
    peer2->set_port("2");

    auto routing_table = std::make_shared<RoutingTable>();
    REQUIRE(routing_table->size() == 0);

    routing_table->set_self(peer1);
    routing_table->set_predecessor(peer1);
    routing_table->set_successor(peer1);

    REQUIRE(routing_table->get_self() == peer1);
    REQUIRE(routing_table->get_predecessor() == peer1);
    REQUIRE(routing_table->size() == 1);

    routing_table->push_back(peer2);
    REQUIRE(routing_table->size() == 2);

    auto peers = routing_table->get_peers();
    REQUIRE(peers[0] == peer1);
    REQUIRE(peers[1] == peer2);
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
