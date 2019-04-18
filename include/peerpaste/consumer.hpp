#ifndef MESSAGE_DISPATCHER
#define CONSUMER_DISPATCHER

#include "peerpaste/concurrent_queue.hpp"
#include "peerpaste/request_object.hpp"
#include "peerpaste/message_converter.hpp"
#include "peerpaste/message.hpp"
#include "peerpaste/session.hpp"

#include <utility>
#include <memory>
#include <vector>

namespace peerpaste {

using MsgPtr = std::unique_ptr<Message>;
using SessionPtr = std::shared_ptr<Session>;
using MsgBufPair = std::pair<std::vector<uint8_t>, SessionPtr>;
using MsgPair = std::pair<MsgPtr, SessionPtr>;

class MessageDispatcher
{
    boost::asio::io_service& service_;
    std::shared_ptr<ConcurrentQueue<MsgBufPair>> queue_;
    std::shared_ptr<ConcurrentQueue<RequestObject>>  send_queue_;
    std::shared_ptr<MessageHandler> msg_handler_;
    bool run_ = true;
public:
    //TODO: should init both queues and provide getter() functions to get shared_ptrs of them
    MessageDispatcher (std::shared_ptr<MessageHandler> msg_handler,
                       boost::asio::io_context& io_context)
        : msg_handler_(msg_handler),
          queue_(std::make_shared<ConcurrentQueue<MsgBufPair>>()),
          send_queue_(std::make_shared<ConcurrentQueue<RequestObject>>()),
          service_(io_context)
    {}

    std::shared_ptr<ConcurrentQueue<MsgBufPair>> get_receive_queue(){
        return queue_;
    }

    std::shared_ptr<ConcurrentQueue<RequestObject>> get_send_queue(){
        return send_queue_;
    }

    /*
     * Should be called to start handling messages. Stoped by calling stop()
     * Pops Msg from Queue, converts it and dispatches it to the MessageHandler
     */
    void run()
    {
        std::thread t1([&]() { run_internal(); });
        t1.detach();
        std::thread t2([&]() { run_send_internal(); });
        t2.detach();
        std::thread t3([&]() { msg_handler_->run(); });
        t3.detach();
    }

    void run_internal()
    {
        ProtobufMessageConverter converter;
        while(run_){
            //Get unique_ptr to MsgPair from queue
            auto msg_pair = queue_->wait_and_pop();
            //Convert msg_buffer into Message object
            //TODO: handle failure on MessageFromSerialized!!!
            auto converted_message = converter.MessageFromSerialized(
                                        std::get<0>(std::move(*msg_pair.get())));
            //create RequestObject
            auto data_object = std::make_unique<RequestObject>();
            //set msg
            converted_message->print();
            data_object->set_message(std::move(converted_message));
            //set conn
            data_object->set_connection(std::move(std::get<1>(*msg_pair.get())));
            //dispatch
            dispatch(std::move(data_object));
        }
    }

    void run_send_internal()
    {
        ProtobufMessageConverter converter;
        while(run_){
            //Get unique_ptr to RequestObject from queue
            auto send_object = send_queue_->wait_and_pop();
            //get the message to send
            auto message = send_object->get_message();
            auto message_is_request = message->is_request();

            //convert message to buf
            converter;
            auto message_buf = converter.SerializedFromMessage(message);
            std::vector<boost::uint8_t> encoded_buf;
            //header size used in session is 4
            encoded_buf.resize(4);
            encode_header(encoded_buf, message_buf.size());
            encoded_buf.insert(encoded_buf.end(),
                               std::make_move_iterator(message_buf.begin()),
                               std::make_move_iterator(message_buf.end())
                               );

            if(send_object->is_session()){
                auto session = send_object->get_session();
                session->write(encoded_buf);
                if(message_is_request){
                    session->read();
                }
            } else {
                //TODO: this is too session/boost specific
                //when using a test_socket or something like that this code
                //should be more abstract so that the message_handler does not need
                //to know what kind of object sends the data somewhere
                auto peer = send_object->get_peer();
                auto write_handler = std::make_shared<Session>(service_, queue_);
                write_handler->write_to(encoded_buf, peer->get_ip(), peer->get_port());
                if(message_is_request){
                    write_handler->read();
                }
            }
            send_object->set_time_point();
        }
    }

    void encode_header(std::vector<boost::uint8_t>& buf, unsigned size) const
    {
        //header size is 4
        assert(buf.size() >= 4);
        buf[0] = static_cast<boost::uint8_t>((size >> 24) & 0xFF);
        buf[1] = static_cast<boost::uint8_t>((size >> 16) & 0xFF);
        buf[2] = static_cast<boost::uint8_t>((size >> 8) & 0xFF);
        buf[3] = static_cast<boost::uint8_t>(size & 0xFF);
    }

    /*
     * Calls a handler function depending on the msg content asynchronously
     * TODO: Should validate msg first
     */
    void dispatch(std::unique_ptr<RequestObject> data_object)
    {
        auto is_request = data_object->is_request();
        /*
         * We have to capture the data_obj by move. otherwise the session shared_ptr
         * will call destructor when this function runs out of scope.
         */
        if(is_request){
            //TODO: is this creating copy of msg_handler_? if so i have to pass std::ref() instead
            std::thread t([this, data_object = std::move(data_object)]() mutable
                { msg_handler_->handle_request(std::move(data_object)); });
            t.join();
        } else {
            std::thread t([this, data_object = std::move(data_object)]() mutable
                { msg_handler_->handle_response(std::move(data_object)); });
            t.join();
        }
    }

    void stop()
    {
        run_ = false;
    }
};

} // closing namespace peerpaste
#endif /* ifndef MESSAGE_DISPATCHER */
