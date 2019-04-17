#ifndef MESSAGE_DISPATCHER
#define CONSUMER_DISPATCHER

#include "peerpaste/concurrent_queue.hpp"
#include "peerpaste/request_object.hpp"
#include "peerpaste/message_converter.hpp"
#include "peerpaste/message.hpp"

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
    std::shared_ptr<ConcurrentQueue<MsgBufPair>> queue_;
    std::shared_ptr<MessageHandler> msg_handler_;
    bool run_ = true;
public:
    MessageDispatcher (std::shared_ptr<ConcurrentQueue<MsgBufPair>> msg_queue,
                       std::shared_ptr<MessageHandler> msg_handler)
        : queue_(msg_queue),
          msg_handler_(msg_handler)
    {}

    /*
     * Should be called to start handling messages. Stoped by calling stop()
     * Pops Msg from Queue, converts it and dispatches it to the MessageHandler
     */
    void run()
    {
        std::thread t([&]() { run_internal(); });
        t.detach();
    }

    void run_internal()
    {
        ProtobufMessageConverter converter;
        while(run_){
            //Get unique_ptr to MsgPair from queue
            auto msg_pair = queue_->wait_and_pop();
            std::cout << "GOT MSG" << std::endl;
            //Convert msg_buffer into Message object
            //TODO: handle failure on MessageFromSerialized!!!
            auto converted_message = converter.MessageFromSerialized(
                                        std::get<0>(std::move(*msg_pair.get())));
            //create RequestObject
            auto data_object = std::make_unique<RequestObject>();
            //set msg
            data_object->set_message(std::move(converted_message));
            //set conn
            data_object->set_connection(std::move(std::get<1>(*msg_pair.get())));
            //dispatch
            dispatch(std::move(data_object));
        }
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
