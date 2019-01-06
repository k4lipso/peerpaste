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
typedef std::vector<boost::uint8_t> data_buffer;
typedef std::shared_ptr<Message> MessagePtr;

class MessageConverter
{
public:
    MessageConverter ();
    virtual ~MessageConverter();

    virtual const MessagePtr MessageFromSerialized(const data_buffer& buf) = 0;
    virtual const MessagePtr SerializedFromMessage(const Message& message) = 0;
};

class ProtobufMessageConverter : MessageConverter
{
public:
    ProtobufMessageConverter ();
    ~ProtobufMessageConverter() {}

    void test(){
        std::cout << "test" << '\n';
    }
    const MessagePtr MessageFromSerialized(const data_buffer& buf) override
    {
        //create a Protobuf Message
        auto protobuf_message = std::make_shared<Request>();
        //fill Message with data by parsing from data_buffer
        protobuf_message->ParseFromArray(&buf, buf.size()); //TODO: could fail

        auto protobuf_header = protobuf_message->commonheader();
        Header header(protobuf_header.t_flag(),
                      protobuf_header.ttl(),
                      protobuf_header.message_length(),
                      protobuf_header.request_type(),
                      protobuf_header.transaction_id(),
                      protobuf_header.version(),
                      protobuf_header.response_code());

        MessagePtr message = std::make_shared<Message>();
        message->set_header(header);

        const int protobuf_peer_size = protobuf_message->peerinfo_size();
        for(int i = 0; i < protobuf_peer_size; i++){
            auto protobuf_peer = protobuf_message->peerinfo(i);
            Peer peer(protobuf_peer.peer_id(),
                      protobuf_peer.peer_ip(),
                      protobuf_peer.peer_port());
            message->add_peer(peer);
        }
        std::cout << "lo" << std::endl;

        return message;
    }

    const MessagePtr SerializedFromMessage(const Message& message) override
    {}

private:
    /* data */
};

#endif /* MESSAGE_BUILDER_HPP */
