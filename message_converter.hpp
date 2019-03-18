#ifndef MESSAGE_BUILDER_HPP
#define MESSAGE_BUILDER_HPP

#include "message.hpp"

#include <string>
#include <vector>
#include <memory>
#include <iostream>

#include <boost/asio.hpp>
#include "proto/messages.pb.h"
#include <google/protobuf/util/delimited_message_util.h>

//TODO: non boost possible?
typedef std::vector<boost::uint8_t> DataBuffer;
typedef std::shared_ptr<Message> MessagePtr;

class MessageConverter
{
public:
    virtual ~MessageConverter() {}

    virtual std::unique_ptr<Message> MessageFromSerialized(const DataBuffer& buf) const = 0;
    virtual const DataBuffer SerializedFromMessage(const MessagePtr message) const = 0;
};

class ProtobufMessageConverter : public MessageConverter
{
public:
    //TODO: error handling! how to proceed on error?
    std::unique_ptr<Message> MessageFromSerialized(const DataBuffer& buf) const override
    {
        //create a Protobuf Message
        auto protobuf_message = std::make_unique<Request>();
        //fill Message with data by parsing from DataBuffer
        protobuf_message->ParseFromArray(&buf[0], buf.size()); //TODO: could fail

        //Get protobuf_header and create Header from it
        auto protobuf_header = protobuf_message->commonheader();
        Header header(protobuf_header.t_flag(),
                      protobuf_header.ttl(),
                      protobuf_header.message_length(),
                      protobuf_header.request_type(),
                      protobuf_header.transaction_id(),
                      protobuf_header.correlational_id(),
                      protobuf_header.version(),
                      protobuf_header.response_code());

        //Create MessagePtr and set the header
        auto message = std::make_unique<Message>();
        message->set_header(header);

        //Iterate over every protobuf_peer, create Peer object from it
        //and add it to our message
        const int protobuf_peer_size = protobuf_message->peerinfo_size();
        for(int i = 0; i < protobuf_peer_size; i++){
            auto protobuf_peer = protobuf_message->peerinfo(i);
            Peer peer(protobuf_peer.peer_id(),
                      protobuf_peer.peer_ip(),
                      protobuf_peer.peer_port());
            message->add_peer(peer);
        }

        return message;
    }

    const DataBuffer SerializedFromMessage(const MessagePtr message) const override
    {
        auto protobuf_message = std::make_unique<Request>();
        auto protobuf_header = protobuf_message->mutable_commonheader();
        auto peerpaste_header = message->get_header();
        auto peerpaste_peers = message->get_peers();

        protobuf_header->set_t_flag(peerpaste_header.get_t_flag());
        protobuf_header->set_ttl(peerpaste_header.get_ttl());
        protobuf_header->set_message_length(peerpaste_header.get_message_length());
        protobuf_header->set_request_type(peerpaste_header.get_request_type());
        protobuf_header->set_transaction_id(peerpaste_header.get_transaction_id());
        protobuf_header->set_correlational_id(peerpaste_header.get_correlational_id());
        protobuf_header->set_version(peerpaste_header.get_version());
        protobuf_header->set_response_code(peerpaste_header.get_response_code());

        for(const auto peer : peerpaste_peers){
            auto protobuf_peer = protobuf_message->add_peerinfo();
            protobuf_peer->set_peer_id(peer.get_id());
            protobuf_peer->set_peer_ip(peer.get_ip());
            protobuf_peer->set_peer_port(peer.get_port());
        }

        std::vector<uint8_t> buf;
        unsigned protobuf_message_size = protobuf_message->ByteSize();
        buf.resize(protobuf_message_size);
        protobuf_message->SerializeToArray(&buf[0], protobuf_message_size);
        return buf;
    }

private:
    /* data */
};

#endif /* MESSAGE_BUILDER_HPP */
